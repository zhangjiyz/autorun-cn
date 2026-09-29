#include "game_directories.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    char root[] = "/tmp/autorun-game-dir-XXXXXX";
    char game[512], executable[512], catalog[512], link[512], resolved[512];
    struct stat st;

    assert( mkdtemp( root ) );
    snprintf( game, sizeof(game), "%s/game", root );
    snprintf( executable, sizeof(executable), "%s/game.exe", game );
    snprintf( catalog, sizeof(catalog), "%s/Data/catalog", game );
    snprintf( link, sizeof(link), "%s/link", game );
    assert( !mkdir( game, 0700 ) );
    assert( game_directory_ensure( executable, "Data/catalog", resolved, sizeof(resolved) ) );
    assert( !strcmp( resolved, catalog ) && !stat( catalog, &st ) && S_ISDIR( st.st_mode ) );
    assert( game_directory_ensure( executable, "Data/catalog", resolved, sizeof(resolved) ) );
    assert( !symlink( root, link ) );
    assert( !game_directory_ensure( executable, "link/escape", resolved, sizeof(resolved) ) );
    for (size_t i = 0; i < 7; i++)
    {
        static const char *bad[] = {"../escape", "/absolute", "Data//bad", "Data/./bad",
                                    "Data/catalog/..", "C:/bad", "Data\\bad"};
        assert( !game_directory_ensure( executable, bad[i], resolved, sizeof(resolved) ) );
        assert( errno == EINVAL );
    }
    assert( !game_directory_ensure( executable, "Data/too-long", resolved, 8 ) );
    assert( errno == ENAMETOOLONG );
    assert( !unlink( link ) );
    assert( !rmdir( catalog ) );
    snprintf( catalog, sizeof(catalog), "%s/Data", game );
    assert( !rmdir( catalog ) );
    assert( !rmdir( game ) );
    assert( !rmdir( root ) );
    return 0;
}
