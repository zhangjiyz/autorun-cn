/* Policy selection: default to CN, allow explicit upstream selection,
 * and keep active handles on one backend until the process restarts. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../source/sd_cache.h"

static unsigned int cn_installs, upstream_installs, upstream_flushes;

int wine_nx_sd_cache_cn_install(void) { cn_installs++; return 1; }
int wine_nx_sd_cache_upstream_install(void) { upstream_installs++; return 1; }
void wine_nx_sd_cache_upstream_flush(void) { upstream_flushes++; }
unsigned int wine_nx_sd_cache_cn_mb(void) { return 32; }
unsigned int wine_nx_sd_cache_upstream_mb(void) { return 64; }

static void settings( const char *json )
{
    FILE *file = fopen( "sdmc:/switch/wine/config/settings.json", "w" );
    assert( file );
    assert( fputs( json, file ) >= 0 );
    assert( !fclose( file ) );
}

int main( int argc, char **argv )
{
    char dir[] = "/tmp/wine-nx-sd-policy.XXXXXX";
    int cn;

    assert( argc == 3 );
    cn = atoi( argv[2] );
    assert( mkdtemp( dir ) );
    assert( !chdir( dir ) );
    assert( !mkdir( "sdmc:", 0700 ) );
    assert( !mkdir( "sdmc:/switch", 0700 ) );
    assert( !mkdir( "sdmc:/switch/wine", 0700 ) );
    assert( !mkdir( "sdmc:/switch/wine/config", 0700 ) );
    if (strcmp( argv[1], "-" )) settings( argv[1] );

    wine_nx_sd_cache_flush();
    assert( !upstream_flushes );
    assert( wine_nx_sd_cache_install() == 1 );
    assert( wine_nx_sd_cache_is_cn() == cn );
    assert( cn_installs == (unsigned int)cn );
    assert( upstream_installs == (unsigned int)!cn );
    assert( wine_nx_sd_cache_mb() == (cn ? 32 : 64) );
    wine_nx_sd_cache_flush();
    assert( upstream_flushes == (unsigned int)!cn );

    settings( cn ? "{\"sd-cn-strategy\":false}" : "{\"sd-cn-strategy\":true}" );
    assert( !wine_nx_sd_cache_install() );
    assert( wine_nx_sd_cache_is_cn() == cn );
    assert( cn_installs + upstream_installs == 1 );

    assert( !unlink( "sdmc:/switch/wine/config/settings.json" ) );
    assert( !rmdir( "sdmc:/switch/wine/config" ) );
    assert( !rmdir( "sdmc:/switch/wine" ) );
    assert( !rmdir( "sdmc:/switch" ) );
    assert( !rmdir( "sdmc:" ) );
    assert( !chdir( "/" ) );
    assert( !rmdir( dir ) );
    printf( "SD policy: %s, restart required to change backend: ok\n", cn ? "CN" : "upstream" );
    return 0;
}
