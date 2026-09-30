#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

static int test_rename( const char *, const char * );
#define rename test_rename
#include "../source/game_profiles.c"
#undef rename

static int crash_after, fail_after, renames;
static int test_rename( const char *from, const char *to )
{
    if (++renames == fail_after) { errno = EIO; return -1; }
    int result = rename( from, to );
    if (!result && renames == crash_after) _exit( 73 );
    return result;
}

struct fixture { char settings[768], keys[768], dll[768], alias[768]; };
static void fixture( struct fixture *f, int existing )
{
    char root[] = "/tmp/swordman-profile-XXXXXX";
    assert( mkdtemp( root ) );
    snprintf( f->settings, sizeof(f->settings), "%s/Renamed.wine-nx.txt", root );
    snprintf( f->keys, sizeof(f->keys), "%s/Renamed.keys.txt", root );
    snprintf( f->dll, sizeof(f->dll), "%s/mss32.dll", root );
    snprintf( f->alias, sizeof(f->alias), "%s/mss32_autorun_original.dll", root );
    if (existing) assert( durable_write( f->dll, "existing player DLL", 19 ) );
}

static void content( const char *path, const void *bytes, size_t length )
{
    FILE *file = fopen( path, "rb" ); assert( file );
    unsigned char buffer[4096];
    size_t used = 0, count;
    while ((count = fread( buffer, 1, sizeof(buffer), file )))
    {
        assert( used + count <= length && !memcmp( buffer, (const unsigned char *)bytes + used, count ) );
        used += count;
    }
    assert( used == length && !ferror( file ) && !fclose( file ) );
}

static void original( const struct fixture *f, int existing )
{
    if (existing) content( f->dll, "existing player DLL", 19 );
    else assert( access( f->dll, F_OK ) && errno == ENOENT );
    assert( access( f->alias, F_OK ) && errno == ENOENT );
}

int main( int argc, char **argv )
{
    assert( argc == 2 );
    struct game_profile_catalog *catalog = calloc( 1, sizeof(*catalog) ); assert( catalog );
    assert( game_profiles_load( argv[1], catalog ) == GAME_PROFILE_OK && catalog->count == 1 );
    const struct game_profile *profile = &catalog->entries[0];
    assert( !strcmp( profile->id, "swordman" ) && profile->min_api == 17 );
    const struct game_profile_file *dll = NULL;
    for (unsigned int i = 0; i < profile->file_count; i++)
        if (!strcmp( profile->files[i].path, "mss32.dll" )) dll = &profile->files[i];
    assert( dll && dll->size == 331776 &&
        !strcmp( dll->digest, "f2d040218d7e63f83c799b005f688c1b4b150994442996b4c4929a46a1f15743" ) );
    int preserved;
    for (int existing = 0; existing <= 1; existing++)
    {
        struct fixture f; fixture( &f, existing ); renames = 0;
        assert( game_profile_apply( f.settings, f.keys, profile, "", "", &preserved ) == GAME_PROFILE_OK );
        int boundaries = renames;
        content( f.dll, dll->data, dll->size );
        assert( access( f.alias, F_OK ) && errno == ENOENT );
        assert( game_profile_restore( f.settings, f.keys ) == GAME_PROFILE_OK ); original( &f, existing );
        for (int i = 1; i <= boundaries; i++)
        {
            struct fixture interrupted; fixture( &interrupted, existing );
            pid_t pid = fork(); assert( pid >= 0 );
            if (!pid)
            {
                renames = 0; crash_after = i;
                game_profile_apply( interrupted.settings, interrupted.keys, profile, "", "", &preserved ); _exit( 0 );
            }
            int status; assert( waitpid( pid, &status, 0 ) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 73 );
            assert( game_profile_recover( interrupted.settings, interrupted.keys ) == GAME_PROFILE_OK );
            original( &interrupted, existing );
            struct fixture failed; fixture( &failed, existing ); renames = 0; fail_after = i;
            assert( game_profile_apply( failed.settings, failed.keys, profile, "", "", &preserved ) == GAME_PROFILE_IO );
            fail_after = 0; original( &failed, existing );
        }
    }
    /* Reinstalling needs no original DLL; old manual aliases are neither read nor changed. */
    struct fixture f; fixture( &f, 0 );
    assert( durable_write( f.alias, "unrelated old alias", 19 ) );
    for (int i = 0; i < 2; i++)
    {
        assert( game_profile_apply( f.settings, f.keys, profile, "", "", &preserved ) == GAME_PROFILE_OK );
        content( f.dll, dll->data, dll->size ); content( f.alias, "unrelated old alias", 19 );
    }
    game_profiles_clear( catalog ); free( catalog );
    puts( "Swordman complete DLL: absent/copy, existing/backup/overwrite, restore, reinstall and all interruption boundaries passed" );
    return 0;
}
