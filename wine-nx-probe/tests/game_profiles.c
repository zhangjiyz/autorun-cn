#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static int test_rename( const char *, const char * );
#define rename test_rename
#include "../source/game_profiles.c"
#undef rename
static int crash_after, rename_count, fail_after;
static int test_rename( const char *from, const char *to )
{
    rename_count++;
    if (fail_after && rename_count == fail_after) { errno = EIO; return -1; }
    int result = rename( from, to );
    if (!result && crash_after == rename_count) _exit( 73 );
    return result;
}

static void set( struct launcher_kv *kv, const char *key, const char *value ) { assert( launcher_kv_set( kv, key, value ) ); }
static void expect( const char *path, const char *key, const char *expected )
{
    struct launcher_kv kv;
    char value[256];
    assert( launcher_kv_load( &kv, path ) );
    const char *found = launcher_kv_get( &kv, key, value, sizeof(value) );
    if (expected) assert( found && !strcmp( expected, value ) );
    else assert( !found );
}
static void binding_version( const char *path, unsigned int version )
{
    struct game_profile_binding b;
    assert( game_profile_binding_read( path, &b ) == GAME_PROFILE_OK );
    assert( b.version == version );
}
int main( int argc, char **argv )
{
    assert( argc >= 2 );
    struct game_profile_catalog *catalog = calloc( 1, sizeof(*catalog) );
    assert( game_profiles_load( argv[1], catalog ) == GAME_PROFILE_OK );
    assert( catalog->count == 2 );
    assert( game_profile_matches( &catalog->entries[0], "新仙剑" ) );
    assert( game_profile_matches( &catalog->entries[0], "NEWpal" ) );
    assert( !game_profile_matches( &catalog->entries[0], "not-this-game" ) );
    for (int i = 2; i < argc; i++)
    {
        if (game_profiles_load( argv[i], catalog ) == GAME_PROFILE_OK)
        { fprintf( stderr, "invalid fixture accepted: %s\n", argv[i] ); assert( 0 ); }
    }
    assert( game_profiles_load( argv[1], catalog ) == GAME_PROFILE_OK );
    struct game_profile profile = catalog->entries[0];
    profile.version = 1;
    const unsigned int initial_version = profile.version;
    char folder[] = "/tmp/autorun-profile-core-XXXXXX", settings[768], keys[768];
    assert( mkdtemp( folder ) );
    char resolved[PATH_MAX]; assert( realpath( folder, resolved ) );
    snprintf( settings, sizeof(settings), "%s/Game.wine-nx.txt", resolved );
    snprintf( keys, sizeof(keys), "%s/Game.keys.txt", resolved );
    assert( durable_write( settings, "# keep comment\ntitle=My game\ncontroller=auto\n", strlen( "# keep comment\ntitle=My game\ncontroller=auto\n" ) ) );
    assert( durable_write( keys, "A=0x20\n", 7 ) );
    int preserved;
    assert( game_profile_apply( settings, keys, &profile, "owner/repo", "", &preserved ) == GAME_PROFILE_OK );
    expect( settings, "title", "My game" ); expect( settings, "controller", "keyboard" ); expect( keys, "A", "0x0d" );
    binding_version( settings, initial_version );
    assert( game_profile_apply( settings, keys, &profile, "owner/repo", "", &preserved ) == GAME_PROFILE_OLD );
    /* A changed value, a deleted value, an unknown setting and an unchanged default. */
    struct launcher_kv kv;
    assert( launcher_kv_load( &kv, settings ) ); set( &kv, "window-fit", "0" ); set( &kv, "verbose", NULL ); set( &kv, "custom-option", "keep" );
    assert( durable_write( settings, kv.text, kv.size ) );
    assert( launcher_kv_load( &kv, keys ) ); set( &kv, "A", "0x41" ); assert( durable_write( keys, kv.text, kv.size ) );
    profile.version = initial_version + 1;
    set( &profile.settings, "window-fit", "1" ); set( &profile.settings, "verbose", "1" );
    set( &profile.settings, "sd-stat-cache", NULL ); set( &profile.settings, "vsync", "0" );
    set( &profile.keys, "A", "0x42" ); set( &profile.keys, "B", "0x08" );
    assert( game_profile_apply( settings, keys, &profile, "owner/repo", "", &preserved ) == GAME_PROFILE_OK );
    assert( preserved == 3 );
    expect( settings, "window-fit", "0" ); expect( settings, "verbose", NULL ); expect( settings, "sd-stat-cache", NULL );
    expect( settings, "vsync", "0" ); expect( settings, "custom-option", "keep" );
    expect( keys, "A", "0x41" ); expect( keys, "B", "0x08" ); binding_version( settings, initial_version + 1 );
    assert( game_profile_restore( settings, keys ) == GAME_PROFILE_OK );
    binding_version( settings, initial_version ); expect( keys, "B", "0x1b" ); expect( keys, "A", "0x41" );
    /* Kill after each rename, including after the durable backup. Recovery must
     * restore both files, baselines and binding, never leave a half-updated game. */
    rename_count = 0;
    assert( game_profile_apply( settings, keys, &profile, "owner/repo", "", &preserved ) == GAME_PROFILE_OK );
    int boundaries = rename_count;
    assert( game_profile_restore( settings, keys ) == GAME_PROFILE_OK );
    for (int step = 1; step <= boundaries; step++)
    {
        pid_t pid = fork(); assert( pid >= 0 );
        if (!pid)
        {
            rename_count = 0; crash_after = step;
            game_profile_apply( settings, keys, &profile, "owner/repo", "", &preserved );
            _exit( 99 );
        }
        int status; assert( waitpid( pid, &status, 0 ) == pid ); assert( WIFEXITED( status ) && WEXITSTATUS( status ) == 73 );
        assert( game_profile_recover( settings, keys ) == GAME_PROFILE_OK );
        binding_version( settings, initial_version ); expect( keys, "B", "0x1b" ); expect( settings, "vsync", NULL );
    }
    rename_count = 0; fail_after = 3;
    assert( game_profile_apply( settings, keys, &profile, "owner/repo", "", &preserved ) == GAME_PROFILE_IO );
    fail_after = 0; binding_version( settings, initial_version ); expect( keys, "B", "0x1b" );
    /* Symlink and malformed rollback records must not be followed or ignored. */
    char outside[768]; snprintf( outside, sizeof(outside), "%s/outside", resolved );
    assert( durable_write( outside, "secret", 6 ) ); assert( !remove( keys ) ); assert( !symlink( outside, keys ) );
    assert( game_profile_apply( settings, keys, &profile, "owner/repo", "", &preserved ) == GAME_PROFILE_IO );
    FILE *f = fopen( outside, "rb" ); char secret[7] = {0}; assert( fread( secret, 1, 6, f ) == 6 ); fclose( f ); assert( !strcmp( secret, "secret" ) );
    assert( !unlink( keys ) ); assert( durable_write( keys, "A=0x41\n", 7 ) );
    struct targets t; assert( target_paths( &t, settings, keys ) );
    assert( durable_write( t.pending, "bad", 3 ) );
    assert( game_profile_recover( settings, keys ) == GAME_PROFILE_RECOVERY );
    assert( !unlink( t.pending ) );
    profile.min_api = GAME_PROFILE_API + 1;
    assert( game_profile_apply( settings, keys, &profile, "owner/repo", "", &preserved ) == GAME_PROFILE_INCOMPATIBLE );
    game_profiles_clear( catalog ); free( catalog );
    printf( "profiles: catalog validation, filtering, binding, merge, rollback, all rename crash boundaries and symlink protection passed\n" );
    return 0;
}
