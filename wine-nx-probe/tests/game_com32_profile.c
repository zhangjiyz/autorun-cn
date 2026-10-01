#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/game_profiles.c"

int main( int argc, char **argv )
{
    struct game_profile_catalog *catalog = calloc( 1, sizeof(*catalog) );
    struct game_profile *profile;
    char name[64];
    assert( argc == 2 && catalog );
    assert( game_profiles_load( argv[1], catalog ) == GAME_PROFILE_OK );
    assert( catalog->count == 1 );
    profile = &catalog->entries[0];
    assert( !strcmp( profile->id, "zero-time-dilemma" ) && profile->min_api == 19 );
    assert( launcher_kv_get( &profile->settings, "register-com32", name, sizeof(name) ) );
    assert( !strcmp( name, "dinput8.dll" ) );
    assert( profile->file_count == 4 );
    for (unsigned int i = 0; i < profile->file_count; i++)
        assert( strcmp( profile->files[i].path, "Register-DInput.exe" ) );
    game_profiles_clear( catalog );
    free( catalog );
    return 0;
}
