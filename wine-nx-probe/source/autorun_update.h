#ifndef WINE_NX_AUTORUN_UPDATE_H
#define WINE_NX_AUTORUN_UPDATE_H

#include <stddef.h>

/* Shared default for runtime, profiles and release packaging. */
#define AUTORUN_DEFAULT_REPOSITORY "PalmMuse/autorun-cn"
#define AUTORUN_RELEASE_PROFILE_INDEX_URL "https://cnb.cool/" AUTORUN_DEFAULT_REPOSITORY "/-/releases/latest/download/autorun-profiles.tsv"
#define AUTORUN_LEGACY_GITHUB_PROFILE_INDEX_URL "https://github.com/zhangjiyz/autorun-cn/releases/latest/download/autorun-profiles.tsv"
#define AUTORUN_LEGACY_GITHUB_DEBUG_INDEX_URL "https://github.com/zhangjiyz/autorun-cn/releases/download/profile-debug/autorun-profiles.tsv"
#ifndef AUTORUN_PROFILE_INDEX_URL
#ifdef AUTORUN_DEBUG_BUILD
#define AUTORUN_PROFILE_INDEX_URL "https://cnb.cool/" AUTORUN_DEFAULT_REPOSITORY "/-/releases/download/profile-debug/autorun-profiles.tsv"
#else
#define AUTORUN_PROFILE_INDEX_URL AUTORUN_RELEASE_PROFILE_INDEX_URL
#endif
#endif

/* Debug builds follow a fixed test Release so they can install prereleases
 * without exposing that channel to production builds. CI may override it. */
#ifndef AUTORUN_RUNTIME_RELEASE_TAG
#ifdef AUTORUN_DEBUG_BUILD
#define AUTORUN_RUNTIME_RELEASE_TAG "profile-debug"
#endif
#endif

enum autorun_update_result
{
    AUTORUN_UPDATE_OK,
    AUTORUN_UPDATE_CANCELLED,
    AUTORUN_UPDATE_NETWORK,
    AUTORUN_UPDATE_NOT_FOUND,
    AUTORUN_UPDATE_INVALID,
    AUTORUN_UPDATE_IO,
    AUTORUN_UPDATE_HASH
};

struct autorun_release
{
    char tag[64];
    char name[128];
    char published[32];
    char notes[16384];
    char url[768];
    char digest[65];
    unsigned long long size;
};

typedef int (*autorun_update_progress)( void *opaque, unsigned long long current,
                                        unsigned long long total );

/* Independent release channels share transport/verification, never installer state. */
struct autorun_update_source
{
    char api[512], prefix[512], asset[128], cache[64];
    unsigned long long max_size;
    long timeout_seconds;
    int allow_prerelease;
};
int autorun_https_url( const char *url );
int autorun_file_matches( const char *path, unsigned long long size, const char *digest );
enum autorun_update_result autorun_update_text( const char *url, char **text, size_t *size,
        long timeout_seconds, autorun_update_progress progress, void *opaque );
enum autorun_update_result autorun_update_file( const char *url, unsigned long long size, const char *digest,
        const char *root, char *path, size_t capacity, long timeout_seconds,
        autorun_update_progress progress, void *opaque );
int autorun_profile_source( struct autorun_update_source *source, const char *repository,
                           const char *tag );
enum autorun_update_result autorun_update_check_source( const struct autorun_update_source *source,
        struct autorun_release *release, autorun_update_progress progress, void *opaque );
enum autorun_update_result autorun_update_download_source( const struct autorun_update_source *source,
        const char *runtime_dir, const struct autorun_release *release, char *path, size_t path_size,
        autorun_update_progress progress, void *opaque );

enum autorun_update_result autorun_update_check( struct autorun_release *release,
        autorun_update_progress progress, void *opaque );
enum autorun_update_result autorun_update_download( const char *runtime_dir,
        const struct autorun_release *release, char *path, size_t path_size,
        autorun_update_progress progress, void *opaque );
const char *autorun_update_error( enum autorun_update_result result );

#endif
