/* Host test for Horizon per-thread message queues (dlls/ntdll/unix/horizon_msg_queue.h). */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../dlls/ntdll/unix/horizon_msg_queue.h"

#define WM_SETTEXT 0x000c
#define WM_USER 0x0400
#define HWND_B 0x10050
#define HWND_A 0x10038

static struct horizon_msgqs queues;

static struct horizon_msgq *queue( unsigned int tid )
{
    return horizon_msgq_get( &queues, tid, NULL );
}

static unsigned int send( unsigned int from, unsigned int to, int type, unsigned int msg, unsigned long long wparam,
                          const void *data, unsigned int size, int has_deadline, unsigned long long deadline )
{
    struct horizon_msgq_sent fields = { .type = type, .win = to == 8 ? HWND_B : HWND_A, .msg = msg, .wparam = wparam };

    return horizon_msgq_send( queue( from ), queue( to ), &fields, data, size, has_deadline, deadline );
}

static unsigned int bits( unsigned int tid )
{
    horizon_msgq_update( queue( tid ), 0 );
    return queue( tid )->wake_bits;
}

/* Process the oldest sent message of "tid" and reply with "value"; returns its msg. */
static unsigned int handle( unsigned int tid, unsigned long long value )
{
    struct horizon_msgq_sent *msg = horizon_msgq_receive( queue( tid ) );
    unsigned int code;

    assert( msg );
    code = msg->msg;
    if (msg->type != HORIZON_MSGQ_MSG_NOTIFY && msg->type != HORIZON_MSGQ_MSG_CALLBACK_RESULT)
        horizon_msgq_reply( queue( tid ), value, 0, 1, NULL, 0 );
    free( msg->data );
    free( msg );
    return code;
}

static unsigned long long reply_of( unsigned int tid, unsigned int *status )
{
    unsigned long long value;
    unsigned char *data;
    unsigned int size;

    *status = horizon_msgq_get_reply( queue( tid ), 1, &value, &data, &size );
    free( data );
    return value;
}

static void test_send_and_reply(void)
{
    struct horizon_msgq_sent *msg;
    unsigned long long value;
    unsigned char *data;
    unsigned int size, status;
    static const char text[] = "reply data";

    /* Thread 4 (A) sends WM_SETTEXT to a window of thread 8 (B). */
    assert( !send( 4, 8, HORIZON_MSGQ_MSG_UNICODE, WM_SETTEXT, 5, NULL, 0, 0, 0 ) );
    assert( bits( 8 ) == HORIZON_MSGQ_QS_SENDMESSAGE && !(bits( 4 ) & HORIZON_MSGQ_QS_SMRESULT) );
    assert( horizon_msgq_get_reply( queue( 4 ), 0, &value, &data, &size ) == HORIZON_MSGQ_STATUS_PENDING );

    msg = horizon_msgq_receive( queue( 8 ) );
    assert( msg && msg->msg == WM_SETTEXT && msg->wparam == 5 && msg->win == HWND_B );
    free( msg );
    assert( !bits( 8 ) );
    /* ReplyMessage answers early; the later reply on return does not change it. */
    assert( !horizon_msgq_reply( queue( 8 ), 111, 0, 0, text, sizeof(text) ) );
    assert( bits( 4 ) & HORIZON_MSGQ_QS_SMRESULT );
    assert( !horizon_msgq_reply( queue( 8 ), 222, 0, 1, NULL, 0 ) );
    assert( !queue( 8 )->recv_result );

    status = horizon_msgq_get_reply( queue( 4 ), 1, &value, &data, &size );
    assert( status == HORIZON_MSGQ_STATUS_SUCCESS && value == 111 );
    assert( size == sizeof(text) && !memcmp( data, text, size ) );
    free( data );
    assert( !bits( 4 ) && !queue( 4 )->send_results );
}

static void test_notify_and_invalid(void)
{
    static const unsigned char packed[] = { 1, 2, 3 };
    struct horizon_msgq_sent *msg;
    struct horizon_msgq_sent fields = { .type = 6 /* MSG_POSTED */ };

    assert( !send( 4, 8, HORIZON_MSGQ_MSG_NOTIFY, WM_USER, 0, packed, sizeof(packed), 0, 0 ) );
    assert( !queue( 4 )->send_results );
    msg = horizon_msgq_receive( queue( 8 ) );
    assert( msg->type == HORIZON_MSGQ_MSG_NOTIFY && msg->data_size == 3 && msg->data[2] == 3 && !msg->result );
    assert( !queue( 8 )->recv_result );
    free( msg->data );
    free( msg );
    assert( horizon_msgq_send( queue( 4 ), queue( 8 ), &fields, NULL, 0, 0, 0 ) ==
            HORIZON_MSGQ_STATUS_INVALID_PARAMETER );
    assert( !queue( 8 )->sent );
}

static void test_nested_sends(void)
{
    unsigned int status;

    /* A sends to B (1); B, while handling it, sends to A (2); A answers 2 and B answers 1. */
    assert( !send( 4, 8, HORIZON_MSGQ_MSG_UNICODE, WM_USER + 1, 0, NULL, 0, 0, 0 ) );
    {
        struct horizon_msgq_sent *first = horizon_msgq_receive( queue( 8 ) );
        free( first );
    }
    assert( !send( 8, 4, HORIZON_MSGQ_MSG_UNICODE, WM_USER + 2, 0, NULL, 0, 0, 0 ) );
    /* A's wait for its own reply sees the message to process. */
    assert( bits( 4 ) == HORIZON_MSGQ_QS_SENDMESSAGE );
    assert( handle( 4, 20 ) == WM_USER + 2 );
    assert( bits( 8 ) & HORIZON_MSGQ_QS_SMRESULT );
    assert( reply_of( 8, &status ) == 20 && !status );
    assert( !horizon_msgq_reply( queue( 8 ), 10, 0, 1, NULL, 0 ) );
    assert( reply_of( 4, &status ) == 10 && !status );

    /* The newest wait comes first: A waits on B (3), then on thread 12 (4). */
    assert( !send( 4, 8, HORIZON_MSGQ_MSG_UNICODE, WM_USER + 3, 0, NULL, 0, 0, 0 ) );
    assert( !send( 4, 12, HORIZON_MSGQ_MSG_UNICODE, WM_USER + 4, 0, NULL, 0, 0, 0 ) );
    assert( handle( 8, 30 ) == WM_USER + 3 );
    assert( !(bits( 4 ) & HORIZON_MSGQ_QS_SMRESULT) );  /* 3 is answered but 4 is awaited */
    assert( handle( 12, 40 ) == WM_USER + 4 );
    assert( bits( 4 ) & HORIZON_MSGQ_QS_SMRESULT );
    assert( reply_of( 4, &status ) == 40 && !status );
    assert( bits( 4 ) & HORIZON_MSGQ_QS_SMRESULT );    /* now 3's answer is ready */
    assert( reply_of( 4, &status ) == 30 && !status );
    assert( !bits( 4 ) && !queue( 4 )->send_results );
}

static void test_timeouts_and_cancel(void)
{
    struct horizon_msgq_sent *msg;
    unsigned int status;

    /* Not received before the deadline: the message is withdrawn. */
    assert( !send( 4, 8, HORIZON_MSGQ_MSG_UNICODE, WM_USER + 5, 0, NULL, 0, 1, 1000 ) );
    assert( !horizon_msgq_expire( &queues, 999 ) );
    assert( horizon_msgq_expire( &queues, 1000 ) );
    assert( !queue( 8 )->sent && (bits( 4 ) & HORIZON_MSGQ_QS_SMRESULT) );
    reply_of( 4, &status );
    assert( status == HORIZON_MSGQ_STATUS_TIMEOUT );

    /* Received but answered too late: the late reply is dropped. */
    assert( !send( 4, 8, HORIZON_MSGQ_MSG_UNICODE, WM_USER + 6, 0, NULL, 0, 1, 2000 ) );
    msg = horizon_msgq_receive( queue( 8 ) );
    free( msg );
    assert( horizon_msgq_expire( &queues, 2500 ) );
    reply_of( 4, &status );
    assert( status == HORIZON_MSGQ_STATUS_TIMEOUT );
    assert( !horizon_msgq_reply( queue( 8 ), 60, 0, 1, NULL, 0 ) );
    assert( !queue( 8 )->recv_result );

    /* The sender gives up (cancel) before any reply; the receiver still answers. */
    assert( !send( 4, 8, HORIZON_MSGQ_MSG_UNICODE, WM_USER + 7, 0, NULL, 0, 0, 0 ) );
    reply_of( 4, &status );
    assert( status == HORIZON_MSGQ_STATUS_PENDING && !queue( 4 )->send_results );
    assert( handle( 8, 70 ) == WM_USER + 7 );
    assert( !bits( 4 ) );
}

static void test_callback(void)
{
    unsigned long long packed[3] = { 0x1111, 0x2222, 0 }, result;
    struct horizon_msgq_sent *msg;

    assert( !send( 4, 8, HORIZON_MSGQ_MSG_CALLBACK, WM_USER + 8, 0, packed, sizeof(packed), 0, 0 ) );
    assert( queue( 4 )->callback_results && !queue( 4 )->send_results );
    msg = horizon_msgq_receive( queue( 8 ) );
    assert( msg->type == HORIZON_MSGQ_MSG_CALLBACK && !msg->data_size );
    free( msg );
    assert( !horizon_msgq_reply( queue( 8 ), 88, 0, 1, NULL, 0 ) );
    /* The sender gets MSG_CALLBACK_RESULT with the callback, its data and the result. */
    assert( bits( 4 ) == HORIZON_MSGQ_QS_SENDMESSAGE && !queue( 4 )->callback_results );
    msg = horizon_msgq_receive( queue( 4 ) );
    assert( msg->type == HORIZON_MSGQ_MSG_CALLBACK_RESULT && msg->msg == WM_USER + 8 && msg->data_size == sizeof(packed) );
    memcpy( &result, msg->data + HORIZON_MSGQ_CALLBACK_RESULT_OFFSET, sizeof(result) );
    assert( result == 88 && !memcmp( msg->data, packed, 16 ) && !queue( 4 )->recv_result );
    free( msg->data );
    free( msg );
}

static void test_thread_exit(void)
{
    struct horizon_msgq_sent *msg;
    unsigned int status;

    /* The receiver exits with the message still queued, and with one in progress. */
    assert( !send( 4, 16, HORIZON_MSGQ_MSG_UNICODE, WM_USER + 9, 0, NULL, 0, 0, 0 ) );
    horizon_msgq_destroy( &queues, queue( 16 ) );
    reply_of( 4, &status );
    assert( status == HORIZON_MSGQ_STATUS_ACCESS_DENIED );
    assert( !send( 4, 20, HORIZON_MSGQ_MSG_UNICODE, WM_USER + 10, 0, NULL, 0, 0, 0 ) );
    msg = horizon_msgq_receive( queue( 20 ) );
    free( msg );
    horizon_msgq_destroy( &queues, queue( 20 ) );
    reply_of( 4, &status );
    assert( status == HORIZON_MSGQ_STATUS_ACCESS_DENIED );

    /* The sender exits while waiting: the receiver's reply has nobody to go to. */
    assert( !send( 24, 8, HORIZON_MSGQ_MSG_UNICODE, WM_USER + 11, 0, NULL, 0, 0, 0 ) );
    assert( !send( 24, 8, HORIZON_MSGQ_MSG_CALLBACK, WM_USER + 12, 0, NULL, 0, 0, 0 ) );
    horizon_msgq_destroy( &queues, queue( 24 ) );
    assert( handle( 8, 1 ) == WM_USER + 11 );
    assert( handle( 8, 2 ) == WM_USER + 12 );
    assert( !queue( 8 )->sent && !queue( 8 )->recv_result );
}

static void test_bits(void)
{
    struct horizon_msgq *q = queue( 28 );

    q->wake_mask = 0;
    q->changed_mask = HORIZON_MSGQ_QS_POSTMESSAGE | HORIZON_MSGQ_QS_TIMER;
    /* A posted message: wakes a wait on changes. */
    horizon_msgq_touch( q, HORIZON_MSGQ_QS_POSTMESSAGE | HORIZON_MSGQ_QS_ALLPOSTMESSAGE );
    horizon_msgq_update( q, HORIZON_MSGQ_QS_POSTMESSAGE | HORIZON_MSGQ_QS_ALLPOSTMESSAGE );
    assert( horizon_msgq_signaled( q ) );
    /* PeekMessage looked and left it: no longer "changed", still pending. */
    horizon_msgq_begin_get( q, 0x04ff, 0, ~0u );
    assert( !horizon_msgq_signaled( q ) && (q->wake_bits & HORIZON_MSGQ_QS_POSTMESSAGE) );
    q->wake_mask = HORIZON_MSGQ_QS_POSTMESSAGE;  /* MWMO_INPUTAVAILABLE */
    assert( horizon_msgq_signaled( q ) );
    q->wake_mask = 0;
    /* A timer falling due appears as a new bit; staying due is not new again. */
    horizon_msgq_update( q, HORIZON_MSGQ_QS_POSTMESSAGE | HORIZON_MSGQ_QS_ALLPOSTMESSAGE | HORIZON_MSGQ_QS_TIMER );
    assert( horizon_msgq_signaled( q ) );
    horizon_msgq_begin_get( q, 0x04ff, 0, ~0u );
    horizon_msgq_update( q, HORIZON_MSGQ_QS_POSTMESSAGE | HORIZON_MSGQ_QS_ALLPOSTMESSAGE | HORIZON_MSGQ_QS_TIMER );
    assert( !horizon_msgq_signaled( q ) );
    /* A filtered PeekMessage keeps ALLPOSTMESSAGE changed. */
    horizon_msgq_touch( q, HORIZON_MSGQ_QS_POSTMESSAGE | HORIZON_MSGQ_QS_ALLPOSTMESSAGE );
    horizon_msgq_begin_get( q, 0x04ff, WM_USER, WM_USER );
    assert( q->changed_bits == HORIZON_MSGQ_QS_ALLPOSTMESSAGE );
    horizon_msgq_update( q, 0 );
    assert( !q->wake_bits );
    /* A message another thread sent and this thread already processed is not
     * new anymore: a wait for changes must block (hardware run of build 15). */
    q->wake_mask = 0;
    q->changed_mask = 0x04ff;  /* MsgWaitForMultipleObjects( QS_ALLINPUT ) */
    assert( !send( 32, 28, HORIZON_MSGQ_MSG_UNICODE, WM_USER + 13, 0, NULL, 0, 0, 0 ) );
    assert( handle( 28, 0 ) == WM_USER + 13 );  /* received before any refresh */
    horizon_msgq_update( q, 0 );
    assert( !q->changed_bits && !horizon_msgq_signaled( q ) );
    assert( !send( 32, 28, HORIZON_MSGQ_MSG_UNICODE, WM_USER + 14, 0, NULL, 0, 0, 0 ) );
    horizon_msgq_update( q, 0 );
    assert( horizon_msgq_signaled( q ) );
    assert( handle( 28, 0 ) == WM_USER + 14 );
    horizon_msgq_update( q, 0 );
    assert( !horizon_msgq_signaled( q ) );
    {
        unsigned int status;
        reply_of( 32, &status );
        reply_of( 32, &status );
    }
    /* WM_QUIT counts as a posted message. */
    horizon_msgq_post_quit( q, 3 );
    horizon_msgq_update( q, 0 );
    assert( q->quit_message && q->exit_code == 3 && (q->wake_bits & HORIZON_MSGQ_QS_POSTMESSAGE) );
    assert( !horizon_msgq_get( &queues, 0, NULL ) );
}

/* Enqueue then refresh exactly as the server does. A key-only wait must
 * still see key input; a raw-input consumer must not wait for a mouse click. */
static void test_hardware_refresh(void)
{
    static const struct { unsigned int message, bit; } cases[] =
    {
        {0x0100, HORIZON_MSGQ_QS_KEY},       /* WM_KEYDOWN */
        {0x0101, HORIZON_MSGQ_QS_KEY},       /* WM_KEYUP */
        {0x0104, HORIZON_MSGQ_QS_KEY},       /* WM_SYSKEYDOWN */
        {0x0105, HORIZON_MSGQ_QS_KEY},       /* WM_SYSKEYUP */
        {0x00ff, HORIZON_MSGQ_QS_RAWINPUT},  /* WM_INPUT */
        {0x00fe, HORIZON_MSGQ_QS_RAWINPUT},  /* WM_INPUT_DEVICE_CHANGE */
        {0x0200, HORIZON_MSGQ_QS_MOUSEMOVE},
        {0x00a0, HORIZON_MSGQ_QS_MOUSEMOVE}, /* WM_NCMOUSEMOVE */
        {0x0201, HORIZON_MSGQ_QS_MOUSEBUTTON},
    };
    struct horizon_msgq q = {0};
    unsigned int i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        q.wake_mask = q.changed_mask = cases[i].bit;
        horizon_msgq_touch( &q, cases[i].bit );
        horizon_msgq_update( &q, horizon_msgq_hardware_bit( cases[i].message ) );
        assert( q.wake_bits == cases[i].bit && q.changed_bits == cases[i].bit );
        assert( horizon_msgq_signaled( &q ) );
        /* PM_NOREMOVE / refresh retains pending input, removal clears it. */
        horizon_msgq_begin_get( &q, cases[i].bit, 0, ~0u );
        horizon_msgq_update( &q, horizon_msgq_hardware_bit( cases[i].message ) );
        assert( q.wake_bits == cases[i].bit && horizon_msgq_signaled( &q ) );
        horizon_msgq_update( &q, 0 );
        assert( !q.wake_bits && !q.changed_bits && !horizon_msgq_signaled( &q ) );
    }
    /* Removing mouse input must not clear keys or raw input still pending. */
    horizon_msgq_update( &q, horizon_msgq_hardware_bit( 0x100 ) |
                           horizon_msgq_hardware_bit( 0xff ) |
                           horizon_msgq_hardware_bit( 0x201 ) );
    horizon_msgq_update( &q, horizon_msgq_hardware_bit( 0x100 ) | horizon_msgq_hardware_bit( 0xff ) );
    assert( q.wake_bits == (HORIZON_MSGQ_QS_KEY | HORIZON_MSGQ_QS_RAWINPUT) );
}

int main(void)
{
    test_send_and_reply();
    test_notify_and_invalid();
    test_nested_sends();
    test_timeouts_and_cancel();
    test_callback();
    test_thread_exit();
    test_bits();
    test_hardware_refresh();
    while (queues.head) horizon_msgq_destroy( &queues, queues.head );
    puts( "Horizon message queues: send/reply, ReplyMessage, notify, nested sends, timeouts, cancel, callbacks, "
          "thread exit and wake bits passed" );
    return 0;
}
