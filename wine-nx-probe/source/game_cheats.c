#include "game_cheats.h"
#include <errno.h>
#include <limits.h>

static int token( const char *text, size_t capacity )
{
    if (!*text || strlen( text ) >= capacity) return 0;
    for (; *text; text++)
        if (!((*text >= 'a' && *text <= 'z') || (*text >= '0' && *text <= '9') ||
              *text == '-' || *text == '_' || *text == '.')) return 0;
    return 1;
}

static int number( const char *text, int32_t *value )
{
    char *end;
    const char *digits = text + (*text == '-');
    if (!*digits || strspn( digits, "0123456789" ) != strlen( digits )) return 0;
    errno = 0;
    long long parsed = strtoll( text, &end, 10 );
    if (errno || *end || parsed < INT32_MIN || parsed > INT32_MAX) return 0;
    *value = parsed;
    return 1;
}

int game_cheat_value_valid( const struct game_cheat *cheat, int64_t value )
{
    return cheat->step > 0 && value >= cheat->minimum && value <= cheat->maximum &&
           (value - cheat->minimum) % cheat->step == 0;
}

int game_cheat_value_parse( const struct game_cheat *cheat, const char *text, int32_t *value )
{
    return number( text, value ) && game_cheat_value_valid( cheat, *value );
}

int game_cheats_parse( const char *text, struct game_cheats *out )
{
    static const char header[] = "autorun-cheats-v1\n";
    memset( out, 0, sizeof(*out) );
    if (!text || strncmp( text, header, sizeof(header) - 1 )) return 0;
    text += sizeof(header) - 1;
    while (*text)
    {
        char line[768], *fields[9];
        size_t length = strcspn( text, "\n" );
        struct game_cheat *c = &out->entries[out->count];
        if (out->count == GAME_CHEATS_MAX || !text[length] || length >= sizeof(line)) return 0;
        memcpy( line, text, length ); line[length] = 0; text += length + 1;
        fields[0] = line;
        for (int i = 1; i < 9; i++)
        {
            char *tab = strchr( fields[i - 1], '\t' );
            if (!tab) return 0;
            *tab = 0; fields[i] = tab + 1;
        }
        for (int i = 0; i < 9; i++) for (const unsigned char *p = (const void *)fields[i]; *p; p++) if (*p < 32) return 0;
        if (!token( fields[0], sizeof(c->id) ) || !*fields[1] || strlen( fields[1] ) >= sizeof(c->name) ||
            strlen( fields[2] ) >= sizeof(c->description) || !token( fields[8], sizeof(c->backend) )) return 0;
        c->integer = !strcmp( fields[3], "integer" );
        if (!c->integer && strcmp( fields[3], "toggle" )) return 0;
        if (!number( fields[4], &c->minimum ) || !number( fields[5], &c->maximum ) ||
            !number( fields[6], &c->step ) || !number( fields[7], &c->initial ) ||
            !game_cheat_value_valid( c, c->initial )) return 0;
        if (!c->integer && (c->minimum != 0 || c->maximum != 1 || c->step != 1 || c->initial != 1)) return 0;
        for (int i = 0; i < out->count; i++) if (!strcmp( out->entries[i].id, fields[0] )) return 0;
        strcpy( c->id, fields[0] ); strcpy( c->name, fields[1] );
        strcpy( c->description, fields[2] ); strcpy( c->backend, fields[8] );
        out->count++;
    }
    return 1;
}

static void state_key( const struct game_cheat *cheat, const char *suffix, char key[80] )
{
    snprintf( key, 80, "cheat.%s.%s", cheat->id, suffix );
}
int game_cheats_enabled( const struct launcher_kv *state )
{
    return launcher_setting_state( state, "enabled" ) == 1;
}
int game_cheat_enabled( const struct launcher_kv *state, const struct game_cheat *cheat )
{
    char key[80]; state_key( cheat, "enabled", key );
    return launcher_setting_state( state, key ) == 1;
}
int32_t game_cheat_value( const struct launcher_kv *state, const struct game_cheat *cheat )
{
    char key[80], text[64]; int32_t value;
    state_key( cheat, "value", key );
    return launcher_kv_get( state, key, text, sizeof(text) ) && game_cheat_value_parse( cheat, text, &value ) ? value : cheat->initial;
}
int game_cheat_set_enabled( struct launcher_kv *state, const struct game_cheat *cheat, int enabled )
{
    char key[80]; state_key( cheat, "enabled", key );
    return launcher_kv_set( state, key, enabled ? "1" : "0" );
}
int game_cheat_set_value( struct launcher_kv *state, const struct game_cheat *cheat, int32_t value )
{
    char key[80], text[32]; state_key( cheat, "value", key );
    if (!game_cheat_value_valid( cheat, value )) return 0;
    snprintf( text, sizeof(text), "%d", (int)value );
    return launcher_kv_set( state, key, text );
}
int game_cheats_reconcile( const struct game_cheats *old, const struct game_cheats *next,
                           const struct launcher_kv *state, int reset, struct launcher_kv *out )
{
    memset( out, 0, sizeof(*out) );
    if (!launcher_kv_set( out, "enabled", !reset && next->count && game_cheats_enabled( state ) ? "1" : "0" )) return 0;
    for (int i = 0; i < next->count; i++)
    {
        const struct game_cheat *c = &next->entries[i], *previous = NULL;
        char key[80], text[64]; int32_t value = c->initial;
        int enabled = 0;
        for (int j = 0; !reset && j < old->count; j++) if (!strcmp( c->id, old->entries[j].id )) { previous = &old->entries[j]; break; }
        if (previous && c->integer == previous->integer && !strcmp( c->backend, previous->backend ))
        {
            state_key( c, "value", key );
            if (launcher_kv_get( state, key, text, sizeof(text) ) && game_cheat_value_parse( c, text, &value ))
                enabled = game_cheat_enabled( state, c );
            else value = c->initial; /* A changed range disables an obsolete selection. */
        }
        if (!game_cheat_set_value( out, c, value ) || !game_cheat_set_enabled( out, c, enabled )) return 0;
    }
    return 1;
}
struct game_cheat_dispatch_result game_cheats_dispatch( const struct game_cheats *definitions,
        const struct launcher_kv *state, game_cheat_apply apply, void *opaque )
{
    struct game_cheat_dispatch_result result = {0};
    if (!game_cheats_enabled( state )) return result;
    for (int i = 0; i < definitions->count; i++)
    {
        const struct game_cheat *c = &definitions->entries[i];
        char key[80], text[64]; int32_t value;
        if (!game_cheat_enabled( state, c )) continue;
        result.requested++;
        state_key( c, "value", key );
        if (!launcher_kv_get( state, key, text, sizeof(text) ) || !game_cheat_value_parse( c, text, &value ) ||
            !apply || !apply( opaque, c, value )) result.unavailable++;
        else result.applied++;
    }
    return result;
}
