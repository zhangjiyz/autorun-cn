/* Composed profile/cheat/cover transaction and legacy recovery regression. */
#include <assert.h>
#include <stdio.h>
#include <stdio.h>
#include <sys/wait.h>
static int test_rename( const char *, const char * );
#define rename test_rename
#include "../source/game_profiles.c"
#undef rename
static int crash_after, renames;
static int test_rename( const char *from, const char *to )
{
    int ok = rename( from, to );
    if (!ok && ++renames == crash_after) _exit( 73 );
    return ok;
}
static int apply( void *opaque, const struct game_cheat *c, int32_t value )
{
    int *calls = opaque; (*calls)++;
    assert( !strcmp( c->id, "test-value" ) && value == 35 ); return 1;
}
static void check_state( const char *settings, int enabled, int value )
{
    struct game_cheats defs; struct launcher_kv state;
    assert( game_profile_cheats_read( settings, &defs, &state ) == GAME_PROFILE_OK );
    assert( defs.count == 2 );
    assert( game_cheats_enabled( &state ) == enabled );
    assert( game_cheat_enabled( &state, &defs.entries[1] ) == enabled );
    assert( game_cheat_value( &state, &defs.entries[1] ) == value );
}
static void check_cover( const char *path, const unsigned char *data, size_t size )
{
    FILE *f = fopen( path, "rb" ); assert( f );
    unsigned char *actual = malloc( size ); assert( actual );
    assert( fread( actual, 1, size, f ) == size && fgetc( f ) == EOF );
    assert( !memcmp( actual, data, size ) ); free( actual ); fclose( f );
}
int main( int argc, char **argv )
{
    assert( argc == 2 );
    struct game_profile_catalog *catalog = calloc( 1, sizeof(*catalog) );
    assert( catalog && game_profiles_load( argv[1], catalog ) == GAME_PROFILE_OK );
    struct game_profile p = catalog->entries[0];
    struct game_cheats defs; assert( game_cheats_parse( p.cheats.text, &defs ) && defs.count == 2 );
    struct game_cheat *number = &defs.entries[1]; int32_t value;
    assert( !game_cheat_value_parse( number, "2147483648", &value ) );
    assert( !game_cheat_value_parse( number, "15abc", &value ) );
    assert( !game_cheat_value_parse( number, "11", &value ) );
    char dir[] = "/tmp/autorun-cheats-XXXXXX"; assert( mkdtemp( dir ) );
    char settings[768], keys[768], second[768], second_keys[768], cover[768];
    snprintf( settings, sizeof(settings), "%s/One.wine-nx.txt", dir );
    snprintf( keys, sizeof(keys), "%s/One.keys.txt", dir );
    snprintf( second, sizeof(second), "%s/Two.wine-nx.txt", dir );
    snprintf( second_keys, sizeof(second_keys), "%s/Two.keys.txt", dir );
    assert( game_profile_cover_path( settings, cover, sizeof(cover) ) );
    int preserved;
    assert( game_profile_apply( settings, keys, &p, "", "", &preserved ) == GAME_PROFILE_OK );
    assert( game_profile_apply( second, second_keys, &p, "", "", &preserved ) == GAME_PROFILE_OK );
    check_state( settings, 0, 10 ); check_cover( cover, p.cover, p.cover_size );
    struct launcher_kv state; assert( game_profile_cheats_read( settings, &defs, &state ) == GAME_PROFILE_OK );
    assert( launcher_kv_set( &state, "enabled", "1" ) && game_cheat_set_enabled( &state, number, 1 ) && game_cheat_set_value( &state, number, 35 ) );
    assert( game_profile_cheats_save( settings, keys, &state ) == GAME_PROFILE_OK );
    check_state( settings, 1, 35 ); check_state( second, 0, 10 );
    int calls = 0;
    struct game_cheat_dispatch_result dispatch = game_cheats_dispatch( &defs, &state, NULL, NULL );
    assert( dispatch.requested == 1 && dispatch.applied == 0 && dispatch.unavailable == 1 );
    dispatch = game_cheats_dispatch( &defs, &state, apply, &calls ); assert( calls == 1 && dispatch.applied == 1 );
    struct launcher_kv off = state; assert( launcher_kv_set( &off, "enabled", "0" ) );
    assert( !game_cheats_dispatch( &defs, &off, apply, &calls ).requested && calls == 1 );
    /* Saving choices must not replace the installation's undo snapshot. */
    assert( game_profile_restore( settings, keys ) == GAME_PROFILE_OK );
    assert( access( settings, F_OK ) && access( cover, F_OK ) );
    assert( game_profile_restore( settings, keys ) == GAME_PROFILE_OK );
    check_state( settings, 1, 35 ); check_cover( cover, p.cover, p.cover_size );
    p.version++;
    assert( game_profile_apply( settings, keys, &p, "", "", &preserved ) == GAME_PROFILE_OK );
    check_state( settings, 1, 35 );
    struct game_cheats changed = defs; changed.entries[1].maximum = 20;
    struct launcher_kv clean;
    assert( game_cheats_reconcile( &defs, &changed, &state, 0, &clean ) );
    assert( !game_cheat_enabled( &clean, &changed.entries[1] ) && game_cheat_value( &clean, &changed.entries[1] ) == 10 );
    strcpy( changed.entries[1].backend, "other.backend" );
    assert( game_cheats_reconcile( &defs, &changed, &state, 0, &clean ) && !game_cheat_enabled( &clean, &changed.entries[1] ) );
    assert( game_cheats_reconcile( &defs, &defs, &state, 1, &clean ) && !game_cheats_enabled( &clean ) );
    /* Switching source resets choices; updating/removing all new resources is atomic. */
    p.version++;
    assert( game_profile_apply( settings, keys, &p, "other/repo", "", &preserved ) == GAME_PROFILE_OK );
    check_state( settings, 0, 10 );
    assert( game_profile_cheats_save( settings, keys, &state ) == GAME_PROFILE_OK );
    struct game_profile next = p; next.version++; next.cover_size = 0; memset( &next.cheats, 0, sizeof(next.cheats) );
    renames = 0;
    assert( game_profile_apply( settings, keys, &next, "other/repo", "", &preserved ) == GAME_PROFILE_OK );
    int boundaries = renames;
    assert( game_profile_restore( settings, keys ) == GAME_PROFILE_OK );
    for (int step = 1; step <= boundaries; step++)
    {
        pid_t pid = fork(); assert( pid >= 0 );
        if (!pid) { renames = 0; crash_after = step; game_profile_apply( settings, keys, &next, "other/repo", "", &preserved ); _exit( 99 ); }
        int status; assert( waitpid( pid, &status, 0 ) == pid && WIFEXITED( status ) && WEXITSTATUS( status ) == 73 );
        assert( game_profile_recover( settings, keys ) == GAME_PROFILE_OK );
        check_state( settings, 1, 35 ); check_cover( cover, p.cover, p.cover_size );
    }
    assert( game_profile_apply( settings, keys, &next, "other/repo", "", &preserved ) == GAME_PROFILE_OK );
    assert( access( cover, F_OK ) );
    assert( game_profile_cheats_read( settings, &defs, &state ) == GAME_PROFILE_OK && !defs.count && !game_cheats_enabled( &state ) );
    /* A v21 pending record can still restore after an NRO upgrade. */
    struct targets t; assert( target_paths( &t, settings, keys ) );
    struct legacy { uint32_t magic, crc, exists[5], sizes[5]; char data[5][LAUNCHER_KV_MAX]; } old = {0};
    old.magic = LEGACY_SNAPSHOT_MAGIC; old.exists[0] = 1;
    strcpy( old.data[0], "title=Legacy\n" ); old.sizes[0] = strlen( old.data[0] );
    old.crc = crc32( 0, (const void *)old.exists, sizeof(old) - offsetof(struct legacy, exists) );
    assert( durable_write( t.pending, &old, sizeof(old) ) );
    assert( game_profile_recover( settings, keys ) == GAME_PROFILE_OK );
    assert( launcher_kv_load( &state, settings ) && !strcmp( state.text, "title=Legacy\n" ) );
    assert( access( t.paths[6], F_OK ) );
    check_state( second, 0, 10 );
    game_profiles_clear( catalog ); free( catalog );
    puts( "cheats/cover: per-game persistence, callback dispatch, migration, backup, binary rollback and v21 recovery passed" );
    return 0;
}
