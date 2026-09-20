/* Copyright 2026 Wine-NX contributors. LGPL-2.1-or-later.
 * Integer geometry shared by the Switch presenter and touchscreen. */
#ifndef __WINE_NX_ASPECT_FIT_H
#define __WINE_NX_ASPECT_FIT_H

struct wine_nx_aspect_rect
{
    int x, y, width, height;
};

static inline int wine_nx_aspect_fit_rect( int source_width, int source_height,
                                           int screen_width, int screen_height,
                                           struct wine_nx_aspect_rect *rect )
{
    if (!rect || source_width <= 0 || source_height <= 0 ||
        source_width > screen_width || source_height > screen_height) return 0;

    if ((long long)screen_width * source_height <= (long long)screen_height * source_width)
    {
        rect->width = screen_width;
        rect->height = (int)((long long)source_height * screen_width / source_width);
    }
    else
    {
        rect->height = screen_height;
        rect->width = (int)((long long)source_width * screen_height / source_height);
    }
    rect->x = (screen_width - rect->width) / 2;
    rect->y = (screen_height - rect->height) / 2;
    return rect->width > 0 && rect->height > 0;
}

static inline int wine_nx_aspect_map( int physical, int offset, int shown, int source )
{
    long long value;

    if (shown <= 0 || source <= 0) return physical;
    value = (long long)(physical - offset) * source / shown;
    if (value < 0) return 0;
    if (value >= source) return source - 1;
    return (int)value;
}

#endif
