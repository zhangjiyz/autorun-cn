/* Targeted regression for both Zhao Yun profiles' hash-pinned local DirectDraw shim. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static int test_rename( const char *, const char * );
#define rename test_rename
#include "../source/game_profiles.c"
#undef rename

static int crash_after, rename_count;
static int test_rename( const char *from, const char *to )
{
    rename_count++;
    int result = rename( from, to );
    if (!result && crash_after == rename_count) _exit( 73 );
    return result;
}

int main( int argc, char **argv )
{
    static const char shim[] = "verified ddraw shim";
    static const char digest[] = "967e70494a1e94db47cfd77038e38e22f4fcd5d00f37a75d8a932494fcf52324";
    char folder[] = "/tmp/autorun-disable-test-XXXXXX", resolved[PATH_MAX], exe[768];
    char target[768], backup[800], state[800], actual[65];
    int changed;
    struct game_profile_catalog *catalog = calloc( 1, sizeof(*catalog) );
    assert( argc == 4 && catalog );
    assert( game_profiles_load( argv[1], catalog ) == GAME_PROFILE_OK && catalog->count == 1 );
    assert( !strcmp( catalog->entries[0].id, argv[2] ) && catalog->entries[0].has_disable_file );
    assert( catalog->entries[0].min_api == (unsigned int)atoi( argv[3] ) );
    assert( !strcmp( catalog->entries[0].disable_digest,
                    "0279b2a2a8d8f208bb0d40131d6ae42cbee9c271f234cb4a25dac9914d0d27b4" ) );
    game_profiles_clear( catalog ); free( catalog );
    assert( mkdtemp( folder ) );
    assert( realpath( folder, resolved ) );
    assert( snprintf( exe, sizeof(exe), "%s/Game.exe", resolved ) < (int)sizeof(exe) );
    assert( disable_paths( exe, target, backup, state ) );
    assert( game_profile_disable_apply( exe, digest, &changed ) == GAME_PROFILE_OK && !changed );
    assert( access( target, F_OK ) && access( backup, F_OK ) && access( state, F_OK ) );
    assert( durable_write( target, shim, sizeof(shim) - 1 ) );
    assert( game_profile_disable_apply( exe, "0000000000000000000000000000000000000000000000000000000000000000", &changed ) == GAME_PROFILE_UNSUPPORTED );
    assert( !access( target, F_OK ) && access( backup, F_OK ) && access( state, F_OK ) );
    assert( game_profile_disable_apply( exe, digest, &changed ) == GAME_PROFILE_OK && changed );
    assert( access( target, F_OK ) && !access( backup, F_OK ) && !access( state, F_OK ) );
    assert( game_profile_disable_apply( exe, digest, &changed ) == GAME_PROFILE_OK && !changed );
    assert( game_profile_disable_restore( exe ) == GAME_PROFILE_OK );
    assert( !access( target, F_OK ) && access( backup, F_OK ) && access( state, F_OK ) );
    assert( !rename( target, backup ) );
    assert( game_profile_disable_apply( exe, digest, &changed ) == GAME_PROFILE_OK && changed );
    assert( game_profile_disable_restore( exe ) == GAME_PROFILE_OK && !access( target, F_OK ) );
    assert( durable_write( backup, "wrong", 5 ) );
    assert( game_profile_disable_apply( exe, digest, &changed ) == GAME_PROFILE_UNSUPPORTED && !changed );
    assert( !access( target, F_OK ) && !unlink( backup ) );
    pid_t pid = fork(); assert( pid >= 0 );
    if (!pid)
    {
        rename_count = 0; crash_after = 1;
        game_profile_disable_apply( exe, digest, &changed );
        _exit( 99 );
    }
    int status; assert( waitpid( pid, &status, 0 ) == pid );
    assert( WIFEXITED( status ) && WEXITSTATUS( status ) == 73 );
    assert( game_profile_disable_recover( exe ) == GAME_PROFILE_OK );
    assert( access( target, F_OK ) && file_digest( backup, actual ) && !strcmp( actual, digest ) );
    assert( game_profile_disable_restore( exe ) == GAME_PROFILE_OK && !access( target, F_OK ) );
    assert( !unlink( target ) && !rmdir( folder ) );
    puts( "Zhao Yun disable rule: ZIP parse, absent file, hash gate, install, repeat, pre-disabled, restore, crash recovery passed" );
    return 0;
}
