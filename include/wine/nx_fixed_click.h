/* A controller press that clicks one fixed point on the Switch screen. */
#ifndef WINE_NX_FIXED_CLICK_H
#define WINE_NX_FIXED_CLICK_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define WINE_NX_FIXED_CLICK_WIDTH 1280
#define WINE_NX_FIXED_CLICK_HEIGHT 720
#define WINE_NX_FIXED_CLICK_QUEUE_SIZE 16

struct wine_nx_fixed_click_point
{
    unsigned short x, y;
    unsigned char enabled, clicks;
};

struct wine_nx_fixed_click_event
{
    int x, y;
};

struct wine_nx_fixed_click_queue
{
    struct wine_nx_fixed_click_event events[WINE_NX_FIXED_CLICK_QUEUE_SIZE];
    unsigned int head, count, held;
};

/* The coordinates are native 1280x720 screen pixels, with (0,0) at top left. */
static inline int wine_nx_fixed_click_parse( const char *value, struct wine_nx_fixed_click_point *point )
{
    char *end;
    unsigned long x, y, clicks = 1;

    if (strncmp( value, "click:", 6 ) || value[6] < '0' || value[6] > '9') return 0;
    errno = 0;
    x = strtoul( value + 6, &end, 10 );
    if (errno || end == value + 6 || *end != ',' || x >= WINE_NX_FIXED_CLICK_WIDTH) return 0;
    errno = 0;
    {
        const char *start = end + 1;
        if (*start < '0' || *start > '9') return 0;
        y = strtoul( start, &end, 10 );
        if (errno || end == start || y >= WINE_NX_FIXED_CLICK_HEIGHT) return 0;
    }
    if (*end == ',')
    {
        const char *start = end + 1;
        if (*start < '0' || *start > '9') return 0;
        errno = 0;
        clicks = strtoul( start, &end, 10 );
        if (errno || end == start || clicks < 1 || clicks > 2) return 0;
    }
    if (*end) return 0;
    point->x = x;
    point->y = y;
    point->enabled = 1;
    point->clicks = clicks;
    return 1;
}

/* Record only the first poll of a press. The Wine thread may take it later. */
static inline void wine_nx_fixed_click_poll( struct wine_nx_fixed_click_queue *queue,
                                             unsigned int key, int down, int deliver,
                                             int x, int y, unsigned int clicks )
{
    unsigned int bit = 1u << key;

    if (!down) { queue->held &= ~bit; return; }
    if (queue->held & bit) return;
    queue->held |= bit;
    if (!deliver) return;
    while (clicks-- && queue->count < WINE_NX_FIXED_CLICK_QUEUE_SIZE)
        queue->events[(queue->head + queue->count++) % WINE_NX_FIXED_CLICK_QUEUE_SIZE] =
            (struct wine_nx_fixed_click_event){x, y};
}

static inline int wine_nx_fixed_click_take( struct wine_nx_fixed_click_queue *queue,
                                             int *x, int *y )
{
    struct wine_nx_fixed_click_event *event;

    if (!queue->count) return 0;
    event = &queue->events[queue->head];
    *x = event->x;
    *y = event->y;
    queue->head = (queue->head + 1) % WINE_NX_FIXED_CLICK_QUEUE_SIZE;
    queue->count--;
    return 1;
}

#endif
