/* Host test for the Horizon server's keyboard input (dlls/ntdll/unix/horizon_keyboard.h). */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../dlls/ntdll/unix/horizon_keyboard.h"
#include "../../include/wine/nx_input_codes.h"

static unsigned char desktop[256], thread[256];
static int alt_pressed;

/* Queue one key as the server does: the event from the state before, then the state after. */
static struct horizon_key_event key( unsigned short vkey, unsigned short scan, unsigned int flags )
{
    struct horizon_key_event event;

    memset( &event, 0xcc, sizeof(event) );
    horizon_keyboard_event( desktop, &alt_pressed, vkey, scan, flags, 0x1234, &event );
    horizon_keyboard_update_state( desktop, event.message, event.vkey, 0xc0 );
    horizon_keyboard_update_state( thread, event.message, event.vkey, 0x80 );
    return event;
}

int main(void)
{
    static const struct horizon_rawinput_device devices[] =
    {
        { 0x00010002, 0, 0x10020 },   /* mouse */
        { HORIZON_RAWINPUT_USAGE_KEYBOARD, HORIZON_RIDEV_NOLEGACY, 0x10024 },
    };
    struct horizon_key_event e;

    /* Enter: press, auto-repeat, release */
    e = key( 0x0d, 0x1c, 0 );
    assert( e.message == HORIZON_KBD_WM_KEYDOWN && e.vkey == 0x0d && e.lparam == 0x001c0001 && !e.data_flags );
    assert( e.raw.make_code == 0x1c && e.raw.flags == 0 && e.raw.vkey == 0x0d && e.raw.message == 0x100 );
    assert( e.raw.extra_information == 0x1234 && !e.raw.reserved );
    assert( desktop[0x0d] == 0xc1 && thread[0x0d] == 0x81 );  /* down, toggled */
    e = key( 0x0d, 0x1c, 0 );
    assert( e.lparam == 0x401c0001 );                          /* previous state: down */
    e = key( 0x0d, 0x1c, HORIZON_KEYEVENTF_KEYUP );
    assert( e.message == HORIZON_KBD_WM_KEYUP && e.lparam == 0xc01c0001 && e.data_flags == 0x80 );
    assert( e.raw.flags == HORIZON_RI_KEY_BREAK && e.raw.message == 0x101 );
    assert( desktop[0x0d] == 0x41 && thread[0x0d] == 0x01 );  /* up; the asynchronous bit stays */

    /* Actual controller mapping -> scan -> server event. MapVirtualKeyEx's
     * null-driver fallback returns keypad aliases without the E0 prefix. */
    {
        static const unsigned int nav[][2] = {
            {0x21,0x49}, {0x22,0x51}, {0x23,0x4f}, {0x24,0x47},
            {0x25,0x4b}, {0x26,0x48}, {0x27,0x4d}, {0x28,0x50}, {0x2d,0x52}, {0x2e,0x53}
        };
        unsigned int i;
        for (i = 0; i < sizeof(nav) / sizeof(nav[0]); i++)
        {
            unsigned int scan = wine_nx_keyboard_scan( nav[i][0], nav[i][1] );
            unsigned int flags = (scan & 0xff00) == 0xe000 ? HORIZON_KEYEVENTF_EXTENDEDKEY : 0;
            e = key( nav[i][0], scan & 0xff, flags );
            assert( e.vkey == nav[i][0] && e.raw.flags == HORIZON_RI_KEY_E0 );
            assert( e.lparam == (0x01000001u | (nav[i][1] << 16)) );
            e = key( nav[i][0], scan & 0xff, flags | HORIZON_KEYEVENTF_KEYUP );
            assert( e.raw.flags == (HORIZON_RI_KEY_E0 | HORIZON_RI_KEY_BREAK) );
        }
        assert( wine_nx_keyboard_scan( 0x68, 0x48 ) == 0x48 ); /* genuine numpad 8 */
        assert( wine_nx_keyboard_scan( 0x0d, 0x1c ) == 0x1c ); /* Enter */
        assert( wine_nx_keyboard_scan( 0x1b, 0x01 ) == 0x01 ); /* Esc */
    }

    /* An arrow is an extended key: DirectInput maps E0 48 to DIK_UP. */
    e = key( 0x26, 0x48, HORIZON_KEYEVENTF_EXTENDEDKEY );
    assert( e.message == HORIZON_KBD_WM_KEYDOWN && e.lparam == 0x01480001 && e.data_flags == 0x01 );
    assert( e.raw.make_code == 0x48 && e.raw.flags == HORIZON_RI_KEY_E0 && e.raw.vkey == 0x26 );
    e = key( 0x26, 0x48, HORIZON_KEYEVENTF_EXTENDEDKEY | HORIZON_KEYEVENTF_KEYUP );
    assert( e.lparam == 0xc1480001 && e.raw.flags == (HORIZON_RI_KEY_E0 | HORIZON_RI_KEY_BREAK) );

    /* Shift: the message carries the left key, raw input the generic one, and both states are down. */
    e = key( HORIZON_KBD_VK_SHIFT, 0x2a, 0 );
    assert( e.vkey == HORIZON_KBD_VK_LSHIFT && e.raw.vkey == HORIZON_KBD_VK_SHIFT );
    assert( (desktop[HORIZON_KBD_VK_LSHIFT] & 0x80) && (desktop[HORIZON_KBD_VK_SHIFT] & 0x80) );
    e = key( HORIZON_KBD_VK_SHIFT, 0x2a, HORIZON_KEYEVENTF_KEYUP );
    assert( !(desktop[HORIZON_KBD_VK_SHIFT] & 0x80) && !(thread[HORIZON_KBD_VK_LSHIFT] & 0x80) );

    /* Alt alone: WM_SYSKEYDOWN, then WM_SYSKEYUP. */
    e = key( HORIZON_KBD_VK_MENU, 0x38, 0 );
    assert( e.message == HORIZON_KBD_WM_SYSKEYDOWN && e.vkey == HORIZON_KBD_VK_LMENU && alt_pressed );
    assert( e.raw.vkey == HORIZON_KBD_VK_MENU && (desktop[HORIZON_KBD_VK_MENU] & 0x80) );
    e = key( HORIZON_KBD_VK_MENU, 0x38, HORIZON_KEYEVENTF_KEYUP );
    assert( e.message == HORIZON_KBD_WM_SYSKEYUP && !alt_pressed && !(desktop[HORIZON_KBD_VK_MENU] & 0x80) );

    /* Alt with another key: both system keys, and the release of Alt is plain. */
    key( HORIZON_KBD_VK_MENU, 0x38, 0 );
    e = key( 'X', 0x2d, 0 );
    assert( e.message == HORIZON_KBD_WM_SYSKEYDOWN && !alt_pressed );
    e = key( 'X', 0x2d, HORIZON_KEYEVENTF_KEYUP );
    assert( e.message == HORIZON_KBD_WM_SYSKEYUP );
    e = key( HORIZON_KBD_VK_MENU, 0x38, HORIZON_KEYEVENTF_KEYUP );
    assert( e.message == HORIZON_KBD_WM_KEYUP );

    /* Ctrl with Alt: no system keys, except releasing Ctrl while Alt is down. */
    e = key( HORIZON_KBD_VK_CONTROL, 0x1d, 0 );
    assert( e.message == HORIZON_KBD_WM_KEYDOWN && e.vkey == HORIZON_KBD_VK_LCONTROL );
    e = key( HORIZON_KBD_VK_MENU, 0x38, 0 );
    assert( e.message == HORIZON_KBD_WM_KEYDOWN );
    e = key( 'X', 0x2d, 0 );
    assert( e.message == HORIZON_KBD_WM_KEYDOWN );
    key( 'X', 0x2d, HORIZON_KEYEVENTF_KEYUP );
    e = key( HORIZON_KBD_VK_CONTROL, 0x1d, HORIZON_KEYEVENTF_KEYUP );
    assert( e.message == HORIZON_KBD_WM_SYSKEYUP && e.raw.vkey == HORIZON_KBD_VK_CONTROL );
    key( HORIZON_KBD_VK_MENU, 0x38, HORIZON_KEYEVENTF_KEYUP );

    /* Right Ctrl is extended. F10 is a system key on its own. */
    e = key( HORIZON_KBD_VK_CONTROL, 0x1d, HORIZON_KEYEVENTF_EXTENDEDKEY );
    assert( e.vkey == HORIZON_KBD_VK_RCONTROL && e.raw.flags == HORIZON_RI_KEY_E0 );
    key( HORIZON_KBD_VK_CONTROL, 0x1d, HORIZON_KEYEVENTF_EXTENDEDKEY | HORIZON_KEYEVENTF_KEYUP );
    assert( !(desktop[HORIZON_KBD_VK_CONTROL] & 0x80) );
    e = key( HORIZON_KBD_VK_F10, 0x44, 0 );
    assert( e.message == HORIZON_KBD_WM_SYSKEYDOWN );

    /* DirectInput 8 registers the keyboard; exclusive access adds RIDEV_NOLEGACY. */
    assert( horizon_rawinput_find( devices, 2, HORIZON_RAWINPUT_USAGE_KEYBOARD ) == &devices[1] );
    assert( horizon_rawinput_find( devices, 1, HORIZON_RAWINPUT_USAGE_KEYBOARD ) == NULL );
    assert( (devices[1].flags & HORIZON_RIDEV_NOLEGACY) == HORIZON_RIDEV_NOLEGACY );
    assert( sizeof(struct horizon_raw_keyboard) == 16 && sizeof(struct horizon_rawinput_device) == 12 );

    puts( "Horizon keyboard: key messages, lParam, repeat, extended keys, modifiers, Alt and F10 rules, key state and raw input passed" );
    return 0;
}
