/* Host test for the SD card read cache (source/sd_read_cache.h). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../source/sd_read_cache.h"

struct fake_file
{
    char *data;
    long long size;
    unsigned int requests;
    int fail;
};

static long long fake_fill( void *ctx, long long offset, char *buf, size_t size )
{
    struct fake_file *f = ctx;

    f->requests++;
    if (f->fail) return -1;
    if (offset >= f->size) return 0;
    if ((long long)size > f->size - offset) size = (size_t)(f->size - offset);
    memcpy( buf, f->data + offset, size );
    return (long long)size;
}

static struct fake_file make_file( long long size )
{
    struct fake_file f = { malloc( (size_t)size ), size, 0, 0 };
    long long i;

    assert( f.data );
    for (i = 0; i < size; i++) f.data[i] = (char)((unsigned long long)i * 2654435761u >> 11);
    return f;
}

static long long cached_read( struct sd_cache_file *file, struct sd_cache_pool *pool, struct fake_file *f,
                              long long offset, char *buf, size_t size )
{
    unsigned int fills = 0, before = f->requests;
    long long got = sd_cache_read( file, pool, offset, buf, size, fake_fill, f, &fills );

    assert( fills == f->requests - before );
    return got;
}

/* OpenTTD's pattern: a seek and a 4 KB read per sprite, mostly forward. */
static void test_sprite_reads(void)
{
    struct sd_cache_pool pool = { 0, 64 };
    struct fake_file f = make_file( 3 * SD_CACHE_CHUNK + 12345 );
    struct sd_cache_file file = { .cacheable = 1 };
    char buf[4096];
    unsigned int reads = 0;
    long long pos;

    for (pos = 0; pos + 4096 <= f.size; pos += 1500, reads++)
    {
        long long at = (reads % 7 == 3 && pos > 50000) ? pos - 50000 : pos;

        assert( cached_read( &file, &pool, &f, at, buf, sizeof(buf) ) == (long long)sizeof(buf) );
        assert( !memcmp( buf, f.data + at, sizeof(buf) ) );
    }
    assert( reads > 250 && f.requests == 4 );  /* one request per chunk */
    assert( pool.used == 4 );
    sd_cache_drop( &file, &pool );
    assert( !pool.used && !file.lines[0].data );
    free( f.data );
}

static void test_end_of_file(void)
{
    struct sd_cache_pool pool = { 0, 64 };
    struct fake_file f = make_file( 1000 );
    struct sd_cache_file file = { .cacheable = 1 };
    char buf[2000];

    assert( cached_read( &file, &pool, &f, 990, buf, 100 ) == 10 && !memcmp( buf, f.data + 990, 10 ) );
    assert( cached_read( &file, &pool, &f, 1000, buf, 100 ) == 0 );
    assert( cached_read( &file, &pool, &f, 5000, buf, 100 ) == 0 );
    assert( cached_read( &file, &pool, &f, 0, buf, sizeof(buf) ) == 1000 && !memcmp( buf, f.data, 1000 ) );
    assert( f.requests == 1 );  /* the short chunk marks the end */
    sd_cache_drop( &file, &pool );
    free( f.data );

    /* A file that ends exactly at a chunk boundary asks once past it. */
    f = make_file( SD_CACHE_CHUNK );
    assert( cached_read( &file, &pool, &f, SD_CACHE_CHUNK - 4, buf, 100 ) == 4 );
    assert( cached_read( &file, &pool, &f, SD_CACHE_CHUNK, buf, 100 ) == 0 );
    assert( cached_read( &file, &pool, &f, SD_CACHE_CHUNK + 50, buf, 100 ) == 0 );
    assert( f.requests == 2 );
    sd_cache_drop( &file, &pool );
    free( f.data );
}

static void test_least_recently_used(void)
{
    struct sd_cache_pool pool = { 0, 64 };
    struct fake_file f = make_file( 20LL * SD_CACHE_CHUNK );
    struct sd_cache_file file = { .cacheable = 1 };
    char buf[100];
    unsigned int i, seed = 1;

    for (i = 0; i < SD_CACHE_LINES; i++)
        assert( cached_read( &file, &pool, &f, (long long)i * SD_CACHE_CHUNK + 7, buf, 10 ) == 10 );
    assert( f.requests == SD_CACHE_LINES );
    assert( cached_read( &file, &pool, &f, 3, buf, 10 ) == 10 && f.requests == SD_CACHE_LINES );
    /* A ninth chunk replaces chunk 1, the least recently used, not chunk 0. */
    assert( cached_read( &file, &pool, &f, 8LL * SD_CACHE_CHUNK, buf, 10 ) == 10 );
    assert( f.requests == SD_CACHE_LINES + 1 && pool.used == SD_CACHE_LINES );
    assert( cached_read( &file, &pool, &f, 11, buf, 10 ) == 10 && f.requests == SD_CACHE_LINES + 1 );
    assert( cached_read( &file, &pool, &f, SD_CACHE_CHUNK, buf, 10 ) == 10 && f.requests == SD_CACHE_LINES + 2 );

    /* Random reads across and between chunks always return the file's bytes. */
    for (i = 0; i < 20000; i++)
    {
        long long at;
        size_t size;

        seed = seed * 1103515245 + 12345;
        at = (long long)(seed % (unsigned int)f.size);
        size = 1 + (seed >> 7) % sizeof(buf);
        if (at + (long long)size > f.size) size = (size_t)(f.size - at);
        assert( cached_read( &file, &pool, &f, at, buf, size ) == (long long)size );
        assert( !memcmp( buf, f.data + at, size ) );
    }
    assert( pool.used == SD_CACHE_LINES );
    sd_cache_drop( &file, &pool );
    free( f.data );
}

static void test_failures_and_memory(void)
{
    struct sd_cache_pool pool = { 0, 64 }, small = { 0, 1 };
    struct fake_file f = make_file( 3LL * SD_CACHE_CHUNK );
    struct sd_cache_file a = { .cacheable = 1 }, b = { .cacheable = 1 };
    char buf[300];

    /* A failed request returns -1, and the next one reuses its buffer. */
    f.fail = 1;
    assert( cached_read( &a, &pool, &f, 5, buf, 10 ) == -1 && pool.used == 1 );
    f.fail = 0;
    assert( cached_read( &a, &pool, &f, 5, buf, 10 ) == 10 && !memcmp( buf, f.data + 5, 10 ) );
    assert( pool.used == 1 );

    /* A request failing after data was copied returns that data. */
    f.fail = 1;
    assert( cached_read( &a, &pool, &f, SD_CACHE_CHUNK - 100, buf, sizeof(buf) ) == 100 );
    assert( !memcmp( buf, f.data + SD_CACHE_CHUNK - 100, 100 ) );
    f.fail = 0;
    sd_cache_drop( &a, &pool );
    assert( !pool.used );

    /* With the pool used up, another file reads directly, and a read that
     * already copied data returns it. */
    assert( cached_read( &a, &small, &f, 5, buf, 10 ) == 10 && small.used == 1 );
    assert( cached_read( &b, &small, &f, 0, buf, 10 ) == SD_CACHE_BYPASS );
    assert( cached_read( &a, &small, &f, SD_CACHE_CHUNK - 100, buf, sizeof(buf) ) == 100 );
    sd_cache_drop( &a, &small );
    assert( cached_read( &b, &small, &f, 0, buf, 10 ) == 10 && small.used == 1 );
    sd_cache_drop( &b, &small );
    assert( !small.used );
    free( f.data );
}

static void test_open_files(void)
{
    static const char grf[] = "sdmc:/switch/wine/drive_c/openttd/baseset/OPENTTD.GRF";
    struct sd_cache_pool pool = { 0, 64 };
    struct fake_file f = make_file( 1000 );
    struct sd_cache_file *list = NULL, *reader, *writer, *late, *after, *lang;
    struct sd_cache_file *legacy_reader, *legacy_writer;
    char buf[10];

    assert( sd_cache_same_path( grf, "/switch//wine/drive_c/openttd/baseset/openttd.grf/" ) );
    assert( sd_cache_same_path( "sdmc:/a/B", "sdmc:/A/b" ) && !sd_cache_same_path( "sdmc:/a/b", "sdmc:/a/bc" ) );
    assert( !sd_cache_same_path( "sdmc:/a/b", "sdmc:/a" ) );

    /* With the per-game option disabled, opening a writer retains the former
     * behavior and drops cached readers of the same path. */
    legacy_reader = sd_cache_opened( &list, &pool, (void *)10, "/legacy.dat", 0, 0 );
    assert( legacy_reader && cached_read( legacy_reader, &pool, &f, 0, buf, sizeof(buf) ) == 10 );
    legacy_writer = sd_cache_opened( &list, &pool, (void *)11, "/LEGACY.DAT", 1, 0 );
    assert( legacy_writer && !legacy_writer->cacheable && !legacy_reader->cacheable && !pool.used );
    sd_cache_closed( &list, &pool, (void *)10 );
    sd_cache_closed( &list, &pool, (void *)11 );

    reader = sd_cache_opened( &list, &pool, (void *)1, grf, 0, 1 );
    assert( reader && reader->cacheable && !reader->writable );
    assert( cached_read( reader, &pool, &f, 0, buf, sizeof(buf) ) == 10 && pool.used == 1 );

    /* Asking for write access alone does not discard clean cached data. Old
     * games often open databases read/write and then only inspect them. */
    writer = sd_cache_opened( &list, &pool, (void *)2, "/switch/wine/drive_c/openttd/baseset/openttd.grf", 1, 1 );
    assert( writer && writer->cacheable && reader->cacheable && reader->lines[0].data && pool.used == 1 );

    /* Readers opened while the writer remains clean can cache too. */
    late = sd_cache_opened( &list, &pool, (void *)3, grf, 0, 1 );
    assert( late && late->cacheable );

    /* The first actual write invalidates all matching handles. */
    sd_cache_modified( list, &pool, (void *)2 );
    assert( writer->dirty && !writer->cacheable && !reader->cacheable && !late->cacheable && !pool.used );
    sd_cache_closed( &list, &pool, (void *)2 );
    assert( !sd_cache_find( list, (void *)2 ) && sd_cache_find( list, (void *)3 ) == late );

    /* After the writer closes, a new reader is cached. */
    after = sd_cache_opened( &list, &pool, (void *)4, grf, 0, 1 );
    assert( after && after->cacheable );

    /* Renaming or removing a path forgets it. */
    lang = sd_cache_opened( &list, &pool, (void *)5, "sdmc:/switch/wine/drive_c/openttd/lang/english.lng", 0, 1 );
    assert( cached_read( lang, &pool, &f, 0, buf, sizeof(buf) ) == 10 && pool.used == 1 );
    sd_cache_forget_path( list, &pool, "sdmc:/switch/wine/drive_c/openttd/lang/ENGLISH.LNG" );
    assert( !lang->cacheable && !pool.used && after->cacheable );

    sd_cache_closed( &list, &pool, (void *)1 );
    sd_cache_closed( &list, &pool, (void *)3 );
    sd_cache_closed( &list, &pool, (void *)4 );
    sd_cache_closed( &list, &pool, (void *)5 );
    sd_cache_closed( &list, &pool, (void *)6 );  /* opened before the cache: unknown */
    assert( !list && !pool.used );
    free( f.data );
}

static int fake_stat( void *ctx, struct stat *st )
{
    struct fake_file *f = ctx;
    f->requests++;
    if (f->fail) return -1;
    memset( st, 0, sizeof(*st) );
    st->st_size = f->size;
    st->st_mtime = 123;
    return 0;
}

static void test_metadata(void)
{
    struct sd_cache_pool pool = {0, 64};
    struct sd_cache_file *list = NULL, *reader, *writer;
    struct fake_file f = {0};
    struct stat st;
    unsigned int queries = 0, i;
    f.size = 123456;
    reader = sd_cache_opened( &list, &pool, (void *)1, "sdmc:/Data/ALL.SND", 0, 1 );
    f.fail = 1;
    assert( sd_cache_read_stat( reader, &st, fake_stat, &f, &queries ) == -1 && !reader->stat_valid );
    f.fail = 0;
    for (i = 0; i < 10000; i++)
    {
        assert( !sd_cache_read_stat( reader, &st, fake_stat, &f, &queries ) );
        assert( st.st_size == 123456 && st.st_mtime == 123 );
    }
    assert( queries == 2 && f.requests == 2 ); /* failed once, then fetched once */
    writer = sd_cache_opened( &list, &pool, (void *)2, "/data/all.snd", 1, 1 );
    assert( writer && writer->stat_cacheable && reader->stat_valid && reader->cacheable );
    f.size = 654321;
    assert( !sd_cache_read_stat( writer, &st, fake_stat, &f, &queries ) && writer->stat_valid );
    assert( !sd_cache_read_stat( writer, &st, fake_stat, &f, &queries ) && st.st_size == f.size );
    assert( queries == 3 );

    /* A real write, unlike read/write access alone, invalidates all matching
     * handles and prevents new metadata caching while that writer is open. */
    sd_cache_modified( list, &pool, (void *)2 );
    assert( writer->dirty && !writer->stat_cacheable && !writer->stat_valid && !reader->stat_cacheable );
    assert( !sd_cache_read_stat( reader, &st, fake_stat, &f, &queries ) && st.st_size == f.size );
    assert( !sd_cache_read_stat( writer, &st, fake_stat, &f, &queries ) && !writer->stat_valid );
    assert( queries == 5 );
    {
        struct sd_cache_file *late = sd_cache_opened( &list, &pool, (void *)3, "/data/all.snd", 0, 1 );
        assert( late && !late->stat_cacheable );
        sd_cache_closed( &list, &pool, (void *)3 );
    }
    sd_cache_closed( &list, &pool, (void *)1 );
    sd_cache_closed( &list, &pool, (void *)2 );
    reader = sd_cache_opened( &list, &pool, (void *)1, "/data/all.snd", 0, 1 );
    assert( !reader->stat_valid ); /* per-open pointer reused */
    assert( !sd_cache_read_stat( reader, &st, fake_stat, &f, &queries ) && reader->stat_valid );
    sd_cache_forget_path( list, &pool, "/data/all.snd" ); /* rename or unlink */
    f.size = 99;
    assert( !reader->stat_valid && !reader->stat_cacheable );
    assert( !sd_cache_read_stat( reader, &st, fake_stat, &f, &queries ) && st.st_size == 99 );
    assert( !sd_cache_read_stat( NULL, &st, fake_stat, &f, &queries ) ); /* untracked / disabled */
    sd_cache_closed( &list, &pool, (void *)1 );
    assert( !list );
}

int main(void)
{
    test_sprite_reads();
    test_end_of_file();
    test_least_recently_used();
    test_failures_and_memory();
    test_open_files();
    test_metadata();
    puts( "SD read cache: sprite reads, end of file, least recently used, failures, memory limit and open "
          "files passed" );
    return 0;
}
