#ifndef WINE_NX_GAME_PROFILES_H
#define WINE_NX_GAME_PROFILES_H

#include "launcher_settings.h"
#include "game_cheats.h"

#define GAME_PROFILE_API 5
#define GAME_PROFILE_COVER_MAX (2u * 1024u * 1024u)
#define GAME_PROFILE_MAX 128
#define GAME_PROFILE_PATCH_MAX 64

struct game_profile_patch
{
    char original_digest[65], patched_digest[65];
    unsigned long long offset;
    unsigned int size;
    unsigned char old_bytes[GAME_PROFILE_PATCH_MAX], new_bytes[GAME_PROFILE_PATCH_MAX];
};

struct game_profile
{
    char id[64], name[96], keywords[192], description[512];
    unsigned int version, min_api;
    char url[768], digest[65];
    unsigned int archive_size;
    struct launcher_kv settings, keys, cheats;
    unsigned char *cover;
    unsigned int cover_size;
    struct game_profile_patch patch;
    int has_cheats, has_cover, has_patch;
};

struct game_profile_catalog
{
    int count;
    struct game_profile entries[GAME_PROFILE_MAX];
};

struct game_profile_binding
{
    char id[64], name[96], repository[768], tag[101];
    unsigned int version;
};

enum game_profile_result
{
    GAME_PROFILE_OK, GAME_PROFILE_INVALID, GAME_PROFILE_IO, GAME_PROFILE_RECOVERY,
    GAME_PROFILE_OLD, GAME_PROFILE_INCOMPATIBLE, GAME_PROFILE_UNSUPPORTED
};

int game_profile_index_parse( const char *text, struct game_profile_catalog *catalog );
int game_profile_archive_name( const struct game_profile *profile, char *out, size_t size );
int game_profile_matches( const struct game_profile *profile, const char *query );
/* Catalogs must be zero initialized; clear releases owned image buffers. */
void game_profiles_clear( struct game_profile_catalog *catalog );
enum game_profile_result game_profiles_load( const char *archive, struct game_profile_catalog *catalog );
/* All target paths come from the launcher, never from the archive. */
enum game_profile_result game_profile_recover( const char *settings, const char *keys );
enum game_profile_result game_profile_patch_recover( const char *exe );
enum game_profile_result game_profile_patch_apply( const char *exe, const struct game_profile_patch *patch, int *changed );
enum game_profile_result game_profile_patch_restore( const char *exe );
enum game_profile_result game_profile_binding_read( const char *settings, struct game_profile_binding *binding );
enum game_profile_result game_profile_apply( const char *settings, const char *keys,
        const struct game_profile *profile, const char *repository, const char *tag, int *preserved );
enum game_profile_result game_profile_restore( const char *settings, const char *keys );
int game_profile_cover_path( const char *settings, char *out, size_t capacity );
enum game_profile_result game_profile_cheats_read( const char *settings, struct game_cheats *definitions,
                                                    struct launcher_kv *state );
enum game_profile_result game_profile_cheats_save( const char *settings, const char *keys,
                                                    const struct launcher_kv *state );
const char *game_profile_error( enum game_profile_result result );

#endif
