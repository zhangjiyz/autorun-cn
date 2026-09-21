#include <assert.h>
#include "../source/autorun_update.c"

static char *read_all( const char *path, size_t *size )
{
    FILE *file = fopen( path, "rb" ); assert( file );
    assert( !fseek( file, 0, SEEK_END ) ); long n = ftell( file ); assert( n > 0 ); rewind( file );
    char *data = malloc( n + 1 ); assert( data && fread( data, 1, n, file ) == (size_t)n );
    fclose( file ); data[n] = 0; *size = n; return data;
}
int main( int argc, char **argv )
{
    assert( argc == 5 );
    struct autorun_update_source source, tagged;
    assert( autorun_profile_source( &source, "owner/repo", "" ) );
    assert( !strcmp( source.api, "https://cnb.cool/owner/repo/-/releases/latest" ) );
    assert( autorun_profile_source( &tagged, AUTORUN_DEFAULT_REPOSITORY, "profiles" ) );
    assert( !strcmp( tagged.api, "https://cnb.cool/PalmMuse/autorun-cn/-/releases/tags/profiles" ) );
    assert( tagged.allow_prerelease );
    assert( !autorun_profile_source( &source, "owner/repo/../../bad", "" ) );
    assert( !autorun_profile_source( &source, "owner/repo?x", "" ) );
    assert( !autorun_profile_source( &source, "owner/repo", "../bad" ) );
    assert( autorun_profile_source( &source, AUTORUN_DEFAULT_REPOSITORY, "" ) );
#ifdef AUTORUN_RUNTIME_RELEASE_TAG
    assert( !strcmp( runtime_source.api, "https://cnb.cool/PalmMuse/autorun-cn/-/releases/tags/" AUTORUN_RUNTIME_RELEASE_TAG ) );
    assert( runtime_source.allow_prerelease );
#else
    assert( !strcmp( runtime_source.api, "https://cnb.cool/PalmMuse/autorun-cn/-/releases/latest" ) );
    assert( !runtime_source.allow_prerelease );
    assert( !strcmp( runtime_source.api, source.api ) );
#endif
    assert( !strcmp( runtime_source.prefix, source.prefix ) );
    assert( !strcmp( runtime_source.asset, "autorun.zip" ) );
    struct autorun_release release;
    size_t size; char *data = read_all( argv[1], &size );
    assert( parse_release( (const void *)data, size, &release, &source ) );
    assert( strstr( release.url, "/autorun-profiles.zip" ) );
    assert( parse_release( (const void *)data, size, &release, &runtime_source ) );
    assert( strstr( release.url, "/autorun.zip" ) );
    free( data );
    data = read_all( argv[2], &size );
    assert( parse_release( (const void *)data, size, &release, &source ) );
    assert( !parse_release( (const void *)data, size, &release, &runtime_source ) );
    free( data );
    data = read_all( argv[3], &size );
    assert( !parse_release( (const void *)data, size, &release, &source ) );
    assert( parse_release( (const void *)data, size, &release, &tagged ) );
#ifdef AUTORUN_RUNTIME_RELEASE_TAG
    assert( parse_release( (const void *)data, size, &release, &runtime_source ) );
#else
    assert( !parse_release( (const void *)data, size, &release, &runtime_source ) );
#endif
    free( data );
    data = read_all( argv[4], &size );
    assert( !parse_release( (const void *)data, size, &release, &source ) );
    assert( !parse_release( (const void *)data, size, &release, &runtime_source ) );
    free( data );
    char out[800];
    memset( &release, 0, sizeof(release) ); strcpy( release.url, "https://cnb.cool/other/repo/-/releases/download/v1/autorun-profiles.zip" );
    assert( autorun_update_download_source( &source, "/tmp", &release, out, sizeof(out), NULL, NULL ) == AUTORUN_UPDATE_INVALID );
    Sha256Context hash;
    sha256ContextCreate( &hash ); sha256ContextUpdate( &hash, "abc", 3 );
    assert( digest_matches( &hash, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ) );
    sha256ContextCreate( &hash ); sha256ContextUpdate( &hash, "abc", 3 );
    assert( !digest_matches( &hash, "0000000000000000000000000000000000000000000000000000000000000000" ) );
    puts( "profile release: exact asset selection, independent channels, source validation and SHA-256 passed" );
    return 0;
}
