/*
 * OpenGL for the Switch display driver
 *
 * Copyright 2026 Wine-NX contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#ifdef __SWITCH__

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "win32u_private.h"
#include "wine/opengl_driver.h"
#include "wine/nx_aspect_fit.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wgl);

/* The runtime hands the screen's NWindow to one OpenGL surface at a time
 * (wine-nx-probe/source/runtime.c). */
extern void *wine_nx_gl_acquire_window( void );
extern void wine_nx_gl_release_window( void );
extern void wine_nx_runtime_trace( const char *msg ) __attribute__((weak));
extern int wine_nx_aspect_source_width __attribute__((weak));
extern int wine_nx_aspect_source_height __attribute__((weak));

static const struct egl_platform *egl;
static const struct opengl_funcs *funcs;
static const struct opengl_drawable_funcs nx_drawable_funcs;

struct nx_gl_drawable
{
    struct opengl_drawable base;
    BOOL screen;  /* this drawable shares the screen's EGL surface */
};

/* The one EGL surface on the screen's NWindow, and the drawables sharing it.
 * win32u gives a window a second drawable whenever another context is made
 * current on it while the first is still in use - wined3d does exactly that
 * when it replaces its caps context with a versioned one - so the drawables
 * of a window share the surface rather than each asking for the screen. */
static pthread_mutex_t nx_screen_mutex = PTHREAD_MUTEX_INITIALIZER;
static EGLSurface nx_screen_surface;
static unsigned int nx_screen_refs;
static int nx_screen_format;

static struct nx_gl_drawable *impl_from_opengl_drawable( struct opengl_drawable *base )
{
    return CONTAINING_RECORD( base, struct nx_gl_drawable, base );
}

static void nx_log( const char *format, ... )
{
    char buffer[256];
    va_list args;

    if (!&wine_nx_runtime_trace) return;
    va_start( args, format );
    vsnprintf( buffer, sizeof(buffer), format, args );
    va_end( args );
    wine_nx_runtime_trace( buffer );
}

/* The EGL config of a pixel format, as win32u's egldrv chooses it for pbuffers. */
static EGLConfig nx_config_for_format( int format )
{
    return egl->configs[(format - 1) % egl->config_count];
}

static void nx_drawable_destroy( struct opengl_drawable *base )
{
    struct nx_gl_drawable *gl = impl_from_opengl_drawable( base );
    EGLSurface surface = NULL;

    if (!gl->screen)
    {
        /* win32u destroys the EGL surface after this callback. */
        return;
    }

    /* The screen's surface outlives this drawable while another one shares it;
     * take it away from win32u either way, so the framebuffer only gets the
     * NWindow back once the surface is really gone. */
    base->surface = NULL;

    pthread_mutex_lock( &nx_screen_mutex );
    if (nx_screen_refs && !--nx_screen_refs)
    {
        surface = nx_screen_surface;
        nx_screen_surface = NULL;
    }
    pthread_mutex_unlock( &nx_screen_mutex );

    if (!surface) return;
    funcs->p_eglDestroySurface( egl->display, surface );
    wine_nx_gl_release_window();
}

static void nx_drawable_flush( struct opengl_drawable *base, UINT flags )
{
    TRACE( "drawable %s, flags %#x\n", debugstr_opengl_drawable( base ), flags );

    if (flags & GL_FLUSH_INTERVAL) funcs->p_eglSwapInterval( egl->display, abs( base->interval ) );
}

/* Frames presented and the time inside eglSwapBuffers, for [PROGRESS]. */
extern unsigned long long horizon_interrupt_time(void);
unsigned int wine_nx_gl_swaps;
unsigned long long wine_nx_gl_swap_time;  /* 100 ns */

/* FBO names belong to an EGL context. Wined3D can replace its caps context
 * with a versioned one while reusing the same screen surface. */
struct nx_aspect_scratch
{
    EGLContext context;
    GLuint texture, framebuffer;
    int width, height;
};
static struct nx_aspect_scratch nx_aspect_scratch[8];
static pthread_mutex_t nx_aspect_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct nx_aspect_scratch *nx_aspect_get_scratch( int width, int height )
{
    EGLContext context = funcs->p_eglGetCurrentContext();
    struct nx_aspect_scratch *slot = NULL;
    GLint old_texture, old_read, old_draw;
    unsigned int i;
    int complete;

    if (!context) return NULL;
    for (i = 0; i < ARRAY_SIZE(nx_aspect_scratch); i++)
    {
        if (nx_aspect_scratch[i].context == context) { slot = &nx_aspect_scratch[i]; break; }
        if (!slot && !nx_aspect_scratch[i].context) slot = &nx_aspect_scratch[i];
    }
    if (!slot) return NULL;
    if (slot->context == context && slot->width == width && slot->height == height &&
        funcs->p_glIsFramebuffer( slot->framebuffer )) return slot;

    /* A destroyed EGL context may reuse its address; its GL names are no
     * longer ours, so replace the record without deleting those names. */
    memset( slot, 0, sizeof(*slot) );
    slot->context = context;
    slot->width = width;
    slot->height = height;
    funcs->p_glGetIntegerv( GL_TEXTURE_BINDING_2D, &old_texture );
    funcs->p_glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &old_read );
    funcs->p_glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &old_draw );
    funcs->p_glGenTextures( 1, &slot->texture );
    funcs->p_glBindTexture( GL_TEXTURE_2D, slot->texture );
    funcs->p_glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    funcs->p_glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    funcs->p_glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
    funcs->p_glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                           GL_RGBA, GL_UNSIGNED_BYTE, NULL );
    funcs->p_glGenFramebuffers( 1, &slot->framebuffer );
    funcs->p_glBindFramebuffer( GL_FRAMEBUFFER, slot->framebuffer );
    funcs->p_glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D, slot->texture, 0 );
    complete = funcs->p_glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
    funcs->p_glBindFramebuffer( GL_READ_FRAMEBUFFER, old_read );
    funcs->p_glBindFramebuffer( GL_DRAW_FRAMEBUFFER, old_draw );
    funcs->p_glBindTexture( GL_TEXTURE_2D, old_texture );
    if (complete) return slot;
    nx_log( "[NXASPECT] OpenGL capture framebuffer incomplete" );
    funcs->p_glDeleteFramebuffers( 1, &slot->framebuffer );
    funcs->p_glDeleteTextures( 1, &slot->texture );
    memset( slot, 0, sizeof(*slot) );
    return NULL;
}

static void nx_aspect_present( int source_width, int source_height )
{
    struct nx_aspect_scratch *scratch;
    struct wine_nx_aspect_rect shown;
    GLfloat clear_color[4];
    GLboolean mask[4], scissor;
    GLint old_read, old_draw, old_read_buffer, old_draw_buffer;

    if (!wine_nx_aspect_fit_rect( source_width, source_height, 1280, 720, &shown )) return;
    pthread_mutex_lock( &nx_aspect_mutex );
    if (!(scratch = nx_aspect_get_scratch( shown.width, shown.height )))
    {
        pthread_mutex_unlock( &nx_aspect_mutex );
        return;
    }
    funcs->p_glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &old_read );
    funcs->p_glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &old_draw );
    funcs->p_glGetIntegerv( GL_READ_BUFFER, &old_read_buffer );
    funcs->p_glGetIntegerv( GL_DRAW_BUFFER, &old_draw_buffer );
    funcs->p_glGetFloatv( GL_COLOR_CLEAR_VALUE, clear_color );
    funcs->p_glGetBooleanv( GL_COLOR_WRITEMASK, mask );
    scissor = funcs->p_glIsEnabled( GL_SCISSOR_TEST );
    funcs->p_glDisable( GL_SCISSOR_TEST );
    funcs->p_glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

    /* Wined3D has already scaled this 4:3 window to the screen height, at
     * the left edge. Capture the whole 960x720 image before moving it. */
    funcs->p_glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
    funcs->p_glReadBuffer( GL_BACK );
    funcs->p_glBindFramebuffer( GL_DRAW_FRAMEBUFFER, scratch->framebuffer );
    funcs->p_glDrawBuffer( GL_COLOR_ATTACHMENT0 );
    funcs->p_glBlitFramebuffer( 0, 0, shown.width, shown.height,
                                0, 0, shown.width, shown.height, GL_COLOR_BUFFER_BIT, GL_NEAREST );

    funcs->p_glBindFramebuffer( GL_READ_FRAMEBUFFER, scratch->framebuffer );
    funcs->p_glReadBuffer( GL_COLOR_ATTACHMENT0 );
    funcs->p_glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
    funcs->p_glDrawBuffer( GL_BACK );
    funcs->p_glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    funcs->p_glClear( GL_COLOR_BUFFER_BIT );
    funcs->p_glBlitFramebuffer( 0, 0, shown.width, shown.height,
                                shown.x, shown.y, shown.x + shown.width, shown.y + shown.height,
                                GL_COLOR_BUFFER_BIT, GL_NEAREST );

    funcs->p_glBindFramebuffer( GL_READ_FRAMEBUFFER, old_read );
    funcs->p_glBindFramebuffer( GL_DRAW_FRAMEBUFFER, old_draw );
    funcs->p_glReadBuffer( old_read_buffer );
    funcs->p_glDrawBuffer( old_draw_buffer );
    funcs->p_glClearColor( clear_color[0], clear_color[1], clear_color[2], clear_color[3] );
    funcs->p_glColorMask( mask[0], mask[1], mask[2], mask[3] );
    if (scissor) funcs->p_glEnable( GL_SCISSOR_TEST );
    pthread_mutex_unlock( &nx_aspect_mutex );
}

static BOOL nx_drawable_swap( struct opengl_drawable *base )
{
    unsigned long long start = horizon_interrupt_time();
    BOOL ret;

    if (impl_from_opengl_drawable( base )->screen && &wine_nx_aspect_source_width &&
        wine_nx_aspect_source_width && &wine_nx_aspect_source_height && wine_nx_aspect_source_height)
        nx_aspect_present( wine_nx_aspect_source_width, wine_nx_aspect_source_height );
    ret = funcs->p_eglSwapBuffers( egl->display, base->surface );

    __atomic_add_fetch( &wine_nx_gl_swap_time, horizon_interrupt_time() - start, __ATOMIC_RELAXED );
    __atomic_add_fetch( &wine_nx_gl_swaps, 1, __ATOMIC_RELAXED );
    return ret;
}

static const struct opengl_drawable_funcs nx_drawable_funcs =
{
    .destroy = nx_drawable_destroy,
    .flush = nx_drawable_flush,
    .swap = nx_drawable_swap,
};

/* A window surface covers the whole screen: the Switch has one NWindow, and
 * window surfaces and EGL cannot share it. Programs drawing with OpenGL are
 * expected to be full screen; other windows are not shown meanwhile. */
static BOOL nx_surface_create( HWND hwnd, BOOL raw, int format, struct opengl_drawable **drawable )
{
    struct opengl_drawable *previous;
    struct client_surface *client;
    struct nx_gl_drawable *gl;
    void *window;

    TRACE( "hwnd %p, raw %u, format %d\n", hwnd, raw, format );
    (void)raw;  /* win32u wraps the drawable in a framebuffer surface itself */

    if ((previous = *drawable) && previous->format == format) return TRUE;
    /* A previous surface may hold the screen: let it go first. */
    if (previous)
    {
        opengl_drawable_release( previous );
        *drawable = NULL;
    }

    if (!(client = nulldrv_client_surface_create( hwnd ))) return FALSE;
    gl = opengl_drawable_create( sizeof(*gl), &nx_drawable_funcs, format, client );
    client_surface_release( client );
    if (!gl) return FALSE;
    gl->base.buffer_map[0] = GL_BACK_LEFT;
    gl->base.buffer_map[1] = GL_BACK_RIGHT;
    gl->base.buffer_map[GL_FRONT - GL_FRONT_LEFT] = GL_BACK;
    gl->base.buffer_map[GL_FRONT_AND_BACK - GL_FRONT_LEFT] = GL_BACK;

    pthread_mutex_lock( &nx_screen_mutex );
    if (nx_screen_surface && nx_screen_format == format)
    {
        /* Another drawable of this window still has the screen; share it. */
        gl->base.surface = nx_screen_surface;
        gl->screen = TRUE;
        nx_screen_refs++;
        pthread_mutex_unlock( &nx_screen_mutex );
        TRACE( "hwnd %p: sharing the screen surface %p\n", hwnd, nx_screen_surface );
        *drawable = &gl->base;
        return TRUE;
    }
    if (nx_screen_surface)
    {
        pthread_mutex_unlock( &nx_screen_mutex );
        ERR( "hwnd %p: the screen has a format %d surface, cannot serve format %d\n",
             hwnd, nx_screen_format, format );
        goto err;
    }
    pthread_mutex_unlock( &nx_screen_mutex );

    if (!(window = wine_nx_gl_acquire_window()))
    {
        ERR( "hwnd %p: the screen already has an OpenGL surface\n", hwnd );
        goto err;
    }
    gl->screen = TRUE;
    if (!(gl->base.surface = funcs->p_eglCreateWindowSurface( egl->display, nx_config_for_format( format ),
                                                              (EGLNativeWindowType)window, NULL )))
    {
        ERR( "hwnd %p: eglCreateWindowSurface failed, error %#x\n", hwnd, funcs->p_eglGetError() );
        /* nothing is sharing the screen yet, so give it back here */
        gl->screen = FALSE;
        wine_nx_gl_release_window();
        goto err;
    }

    pthread_mutex_lock( &nx_screen_mutex );
    nx_screen_surface = gl->base.surface;
    nx_screen_format = format;
    nx_screen_refs = 1;
    pthread_mutex_unlock( &nx_screen_mutex );

    TRACE( "created drawable %s with EGL surface %p\n", debugstr_opengl_drawable( &gl->base ), gl->base.surface );
    {
        static BOOL logged;
        const char *vendor = funcs->p_eglQueryString( egl->display, EGL_VENDOR );
        const char *version = funcs->p_eglQueryString( egl->display, EGL_VERSION );

        /* raw 0: win32u draws through its framebuffer surface (DPI scaling or gamma) */
        if (!logged) nx_log( "[NXGL] EGL %s %s, %u configs; window surface for format %d, raw %u",
                             vendor ? vendor : "?", version ? version : "?", egl->config_count, format, raw );
        logged = TRUE;
    }
    *drawable = &gl->base;
    return TRUE;

err:
    opengl_drawable_release( &gl->base );
    return FALSE;
}

/* win32u's egldrv creates contexts without a config, but switch-mesa 20.1's EGL
 * driver lacks EGL_KHR_no_config_context (such a context fails with
 * EGL_BAD_CONFIG) and EGL_KHR_create_context_no_error. A context gets the config
 * of its pixel format, the one this format's window surfaces and pbuffers use,
 * so EGL lets them be made current together. */
static BOOL nx_context_create( int format, void *share, const int *attribs, void **context )
{
    EGLint egl_attribs[16], *end = egl_attribs, error;

    TRACE( "format %d, share %p, attribs %p\n", format, share, attribs );

    for (; attribs && attribs[0]; attribs += 2)
    {
        EGLint name, *dst;

        switch (attribs[0])
        {
        case WGL_CONTEXT_MAJOR_VERSION_ARB:
            name = EGL_CONTEXT_MAJOR_VERSION_KHR;
            break;
        case WGL_CONTEXT_MINOR_VERSION_ARB:
            name = EGL_CONTEXT_MINOR_VERSION_KHR;
            break;
        case WGL_CONTEXT_FLAGS_ARB:
            name = EGL_CONTEXT_FLAGS_KHR;
            break;
        case WGL_CONTEXT_PROFILE_MASK_ARB:
            if (attribs[1] & WGL_CONTEXT_ES2_PROFILE_BIT_EXT)
            {
                ERR( "OpenGL ES contexts are not supported\n" );
                return FALSE;
            }
            name = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
            break;
        case WGL_CONTEXT_OPENGL_NO_ERROR_ARB:
            FIXME( "no-error contexts are unavailable, ignoring %#x\n", attribs[1] );
            continue;
        default:
            FIXME( "unhandled attribute %#x %#x\n", attribs[0], attribs[1] );
            continue;
        }

        /* A repeated attribute replaces the earlier one. */
        for (dst = egl_attribs; dst != end && *dst != name; dst += 2) continue;
        if (dst == end)
        {
            if (end - egl_attribs >= (int)ARRAY_SIZE(egl_attribs) - 2) continue;
            end += 2;
        }
        dst[0] = name;
        dst[1] = attribs[1];
    }
    *end = EGL_NONE;

    funcs->p_eglBindAPI( EGL_OPENGL_API );
    *context = funcs->p_eglCreateContext( egl->display, nx_config_for_format( format ), share,
                                          end != egl_attribs ? egl_attribs : NULL );
    if ((error = funcs->p_eglGetError()) != EGL_SUCCESS || !*context)
    {
        ERR( "context creation failed for format %d, share %p, error %#x\n", format, share, error );
        return FALSE;
    }
    TRACE( "created context %p\n", *context );
    return TRUE;
}

/* Mesa's Switch platform is the default display; its windows are NWindows. */
static void nx_init_egl_platform( struct egl_platform *platform )
{
    platform->type = 0;
    platform->native_display = 0;
    egl = platform;
}

static struct opengl_driver_funcs nx_driver_funcs =
{
    .p_init_egl_platform = nx_init_egl_platform,
    .p_surface_create = nx_surface_create,
    .p_context_create = nx_context_create,
};

UINT wine_nx_drv_OpenGLInit( UINT version, const struct opengl_funcs *opengl_funcs,
                             const struct opengl_driver_funcs **driver_funcs )
{
    if (version != WINE_OPENGL_DRIVER_VERSION)
    {
        ERR( "version mismatch, opengl32 wants %u but the driver has %u\n", version, WINE_OPENGL_DRIVER_VERSION );
        return STATUS_INVALID_PARAMETER;
    }
    if (!opengl_funcs->egl_handle) return STATUS_NOT_SUPPORTED;
    funcs = opengl_funcs;

    nx_driver_funcs.p_get_proc_address = (*driver_funcs)->p_get_proc_address;
    nx_driver_funcs.p_init_pixel_formats = (*driver_funcs)->p_init_pixel_formats;
    nx_driver_funcs.p_describe_pixel_format = (*driver_funcs)->p_describe_pixel_format;
    nx_driver_funcs.p_init_wgl_extensions = (*driver_funcs)->p_init_wgl_extensions;
    nx_driver_funcs.p_context_destroy = (*driver_funcs)->p_context_destroy;
    nx_driver_funcs.p_make_current = (*driver_funcs)->p_make_current;
    nx_driver_funcs.p_pbuffer_create = (*driver_funcs)->p_pbuffer_create;
    nx_driver_funcs.p_pbuffer_updated = (*driver_funcs)->p_pbuffer_updated;
    nx_driver_funcs.p_pbuffer_bind = (*driver_funcs)->p_pbuffer_bind;

    *driver_funcs = &nx_driver_funcs;
    return STATUS_SUCCESS;
}

#endif /* __SWITCH__ */
