#ifndef GAME_CHEATS_H
#define GAME_CHEATS_H
#include "launcher_settings.h"
#include <stdint.h>

#define GAME_CHEATS_MAX 32
struct game_cheat
{
    char id[48], name[96], description[256], backend[64];
    int integer;
    int32_t minimum, maximum, step, initial;
};
struct game_cheats { int count; struct game_cheat entries[GAME_CHEATS_MAX]; };

/* Definitions are supplied by the profile. State is private to this executable.
 * No definition can enable itself. Backends are explicit callbacks, never code
 * or scripts loaded from a release. */
int game_cheats_parse( const char *text, struct game_cheats *out );
int game_cheat_value_valid( const struct game_cheat *cheat, int64_t value );
int game_cheat_value_parse( const struct game_cheat *cheat, const char *text, int32_t *value );
int game_cheats_enabled( const struct launcher_kv *state );
int game_cheat_enabled( const struct launcher_kv *state, const struct game_cheat *cheat );
int32_t game_cheat_value( const struct launcher_kv *state, const struct game_cheat *cheat );
int game_cheat_set_enabled( struct launcher_kv *state, const struct game_cheat *cheat, int enabled );
int game_cheat_set_value( struct launcher_kv *state, const struct game_cheat *cheat, int32_t value );
int game_cheats_reconcile( const struct game_cheats *old, const struct game_cheats *next,
                           const struct launcher_kv *state, int reset, struct launcher_kv *out );

typedef int (*game_cheat_apply)( void *opaque, const struct game_cheat *cheat, int32_t value );
struct game_cheat_dispatch_result { int requested, applied, unavailable; };
/* A NULL callback is the initial framework: report pending items, perform no writes. */
struct game_cheat_dispatch_result game_cheats_dispatch( const struct game_cheats *definitions,
        const struct launcher_kv *state, game_cheat_apply apply, void *opaque );
#endif
