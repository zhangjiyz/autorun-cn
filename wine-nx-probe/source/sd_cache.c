/* Copyright 2026 Wine-NX contributors. LGPL-2.1-or-later. */
/*
 * The SD card read cache (sd_read_cache.h), installed over libnx's sdmc device
 * before the runtime opens a file on it. Everything that reads the card goes
 * through it: Wine's file system calls, image mapping, NLS files and fonts.
 *
 * Files are tracked by libnx's per-open data, so a file opened before the
 * cache was installed passes straight through. Data is copied to the caller
 * after the lock is released: a fault on the caller's buffer must not happen
 * while the cache is locked.
 */
#include <switch.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/iosupport.h>
#include <unistd.h>

#include "sd_read_cache.h"

/* Reported by the runtime's [PROGRESS] line. */
int wine_nx_sd_stat_cache;             /* per-game opt-in, immutable read-only files */
unsigned int wine_nx_sd_stat_queries, wine_nx_sd_stat_hits;
unsigned int wine_nx_sd_reads;         /* read requests sent to the FS service */
unsigned int wine_nx_sd_hits;          /* reads the cache served without one */
unsigned long long wine_nx_sd_read_ns; /* time spent in those requests */

extern void wine_nx_runtime_trace( const char *msg );

static const devoptab_t *sd_cache_base;
static devoptab_t sd_cache_device;
static pthread_mutex_t sd_cache_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct sd_cache_file *sd_cache_files;
static struct sd_cache_pool sd_cache_pool = { 0, 256 };  /* 32 MB */
static int sd_cache_off;

struct sd_cache_fill_ctx
{
    struct _reent *r;
    void *fd;
};

static ssize_t sd_cache_base_read( struct _reent *r, void *fd, char *ptr, size_t len )
{
    u64 start = armGetSystemTick();
    ssize_t ret = sd_cache_base->read_r( r, fd, ptr, len );

    __atomic_add_fetch( &wine_nx_sd_reads, 1, __ATOMIC_RELAXED );
    __atomic_add_fetch( &wine_nx_sd_read_ns, armTicksToNs( armGetSystemTick() - start ), __ATOMIC_RELAXED );
    return ret;
}

static long long sd_cache_fill( void *ctx, long long offset, char *buf, size_t size )
{
    struct sd_cache_fill_ctx *fill = ctx;

    if (sd_cache_base->seek_r( fill->r, fill->fd, offset, SEEK_SET ) == -1) return -1;
    return sd_cache_base_read( fill->r, fill->fd, buf, size );
}

static int sd_cache_open( struct _reent *r, void *fd, const char *path, int flags, int mode )
{
    int writable = (flags & O_ACCMODE) != O_RDONLY;

    if (sd_cache_base->open_r( r, fd, path, flags, mode ) == -1) return -1;
    pthread_mutex_lock( &sd_cache_mutex );
    if (!sd_cache_opened( &sd_cache_files, &sd_cache_pool, fd, path, writable ) && writable)
    {
        /* Without a record of this writer, its readers cannot be told apart. */
        struct sd_cache_file *file;

        sd_cache_off = 1;
        for (file = sd_cache_files; file; file = file->next) sd_cache_drop( file, &sd_cache_pool );
    }
    pthread_mutex_unlock( &sd_cache_mutex );
    return 0;
}

static int sd_cache_close( struct _reent *r, void *fd )
{
    pthread_mutex_lock( &sd_cache_mutex );
    sd_cache_closed( &sd_cache_files, &sd_cache_pool, fd );
    pthread_mutex_unlock( &sd_cache_mutex );
    return sd_cache_base->close_r( r, fd );
}

/* A read that stops short of what was asked for, before the end of the file,
 * hands a program half of what it wanted with nothing to say so. Halo reports
 * that one of its files is missing or corrupted and never says which, so a card
 * that answers a request with less than it holds has to be visible. */
static void sd_cache_report_short_read( struct _reent *r, void *fd, long long got, size_t len )
{
    static unsigned int reported;
    const struct sd_cache_file *file;
    int failure = errno;
    off_t at, end;
    char message[384];

    if (reported >= 16) return;
    /* A file the program never opened for reading is the runtime's own stdout
     * or stderr, and refusing to read it is not the card holding data back. */
    if (got < 0 && (failure == EBADF || failure == EACCES || failure == EPERM)) return;
    if ((at = sd_cache_base->seek_r( r, fd, 0, SEEK_CUR )) == -1) return;
    if ((end = sd_cache_base->seek_r( r, fd, 0, SEEK_END )) == -1) return;
    sd_cache_base->seek_r( r, fd, at, SEEK_SET );
    /* The file really ending there is a short read of its own. */
    if (got >= 0 && (long long)at >= (long long)end) return;
    reported++;
    pthread_mutex_lock( &sd_cache_mutex );
    file = sd_cache_find( sd_cache_files, fd );
    snprintf( message, sizeof(message), "[FS] %s gave %lld of %u bytes, at %lld of %lld",
              file && file->path ? file->path : "a file on the card", got, (unsigned int)len,
              (long long)at - (got > 0 ? got : 0), (long long)end );
    pthread_mutex_unlock( &sd_cache_mutex );
    wine_nx_runtime_trace( message );
}

static ssize_t sd_cache_read_file( struct _reent *r, void *fd, char *ptr, size_t len )
{
    struct sd_cache_fill_ctx ctx = { r, fd };
    struct sd_cache_file *file;
    unsigned int fills = 0;
    long long got = SD_CACHE_BYPASS;
    char *copy;
    off_t pos;

    if (!len || len >= SD_CACHE_DIRECT || !(copy = malloc( len )))
    {
        ssize_t direct = sd_cache_base_read( r, fd, ptr, len );

        if (len && (size_t)direct != len) sd_cache_report_short_read( r, fd, direct, len );
        return direct;
    }

    pthread_mutex_lock( &sd_cache_mutex );
    if (!sd_cache_off && (file = sd_cache_find( sd_cache_files, fd )) && file->cacheable &&
        (pos = sd_cache_base->seek_r( r, fd, 0, SEEK_CUR )) != -1)
    {
        got = sd_cache_read( file, &sd_cache_pool, pos, copy, len, sd_cache_fill, &ctx, &fills );
        /* The file position ends after the bytes returned, as for a plain read. */
        sd_cache_base->seek_r( r, fd, got >= 0 ? pos + got : pos, SEEK_SET );
    }
    pthread_mutex_unlock( &sd_cache_mutex );

    if (got >= 0)
    {
        memcpy( ptr, copy, (size_t)got );
        if (!fills) __atomic_add_fetch( &wine_nx_sd_hits, 1, __ATOMIC_RELAXED );
    }
    free( copy );
    if (got == SD_CACHE_BYPASS)
    {
        ssize_t direct = sd_cache_base_read( r, fd, ptr, len );

        if ((size_t)direct != len) sd_cache_report_short_read( r, fd, direct, len );
        return direct;
    }
    if ((size_t)got != len) sd_cache_report_short_read( r, fd, got, len );
    return (ssize_t)got;  /* -1 keeps the errno of the failed request */
}

static int sd_cache_query_stat( void *ctx, struct stat *st )
{
    struct sd_cache_fill_ctx *query = ctx;
    return sd_cache_base->fstat_r( query->r, query->fd, st );
}

static int sd_cache_fstat( struct _reent *r, void *fd, struct stat *st )
{
    struct sd_cache_fill_ctx ctx = { r, fd };
    struct stat copy;
    struct sd_cache_file *file;
    unsigned int queries = 0;
    int ret;

    if (!wine_nx_sd_stat_cache)
    {
        __atomic_add_fetch( &wine_nx_sd_stat_queries, 1, __ATOMIC_RELAXED );
        return sd_cache_base->fstat_r( r, fd, st );
    }
    pthread_mutex_lock( &sd_cache_mutex );
    file = !sd_cache_off ? sd_cache_find( sd_cache_files, fd ) : NULL;
    ret = sd_cache_read_stat( file, &copy, sd_cache_query_stat, &ctx, &queries );
    pthread_mutex_unlock( &sd_cache_mutex );
    __atomic_add_fetch( &wine_nx_sd_stat_queries, queries, __ATOMIC_RELAXED );
    if (!queries) __atomic_add_fetch( &wine_nx_sd_stat_hits, 1, __ATOMIC_RELAXED );
    if (!ret) *st = copy; /* Do not fault on the caller's buffer with the lock held. */
    return ret;
}

static int sd_cache_rename( struct _reent *r, const char *old_name, const char *new_name )
{
    pthread_mutex_lock( &sd_cache_mutex );
    sd_cache_forget_path( sd_cache_files, &sd_cache_pool, old_name );
    sd_cache_forget_path( sd_cache_files, &sd_cache_pool, new_name );
    pthread_mutex_unlock( &sd_cache_mutex );
    return sd_cache_base->rename_r( r, old_name, new_name );
}

static int sd_cache_unlink( struct _reent *r, const char *name )
{
    pthread_mutex_lock( &sd_cache_mutex );
    sd_cache_forget_path( sd_cache_files, &sd_cache_pool, name );
    pthread_mutex_unlock( &sd_cache_mutex );
    return sd_cache_base->unlink_r( r, name );
}

static int sd_cache_ftruncate( struct _reent *r, void *fd, off_t len )
{
    struct sd_cache_file *file;

    pthread_mutex_lock( &sd_cache_mutex );
    if ((file = sd_cache_find( sd_cache_files, fd ))) sd_cache_forget_path( sd_cache_files, &sd_cache_pool, file->path );
    pthread_mutex_unlock( &sd_cache_mutex );
    return sd_cache_base->ftruncate_r( r, fd, len );
}

/* Replace the sdmc device in place: its index is the default device, which
 * paths without a device name rely on. Returns 0 when there is no sdmc. */
int wine_nx_sd_cache_install(void)
{
    int device = FindDevice( "sdmc:" );

    if (device < 0 || !devoptab_list[device] || sd_cache_base) return 0;
    sd_cache_base = devoptab_list[device];
    sd_cache_device = *sd_cache_base;
    sd_cache_device.open_r = sd_cache_open;
    sd_cache_device.close_r = sd_cache_close;
    sd_cache_device.read_r = sd_cache_read_file;
    if (sd_cache_base->fstat_r) sd_cache_device.fstat_r = sd_cache_fstat;
    if (sd_cache_base->rename_r) sd_cache_device.rename_r = sd_cache_rename;
    if (sd_cache_base->unlink_r) sd_cache_device.unlink_r = sd_cache_unlink;
    if (sd_cache_base->ftruncate_r) sd_cache_device.ftruncate_r = sd_cache_ftruncate;
    devoptab_list[device] = &sd_cache_device;
    return 1;
}
