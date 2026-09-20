#include <assert.h>
#include <unistd.h>
#include "launcher_cheats.h"
#include "launcher.h"
#include "launcher_ui.h"
#include "launcher_list.h"
#include "game_profiles.h"
static int main_step, detail_step, invalid, prompts;
static const char *root;
static char settings[768];
int launcher_platform_prompt( const char *header, const char *initial, char *out, size_t size )
{
    (void)header; (void)initial;
    snprintf( out, size, "%s", prompts++ ? "35" : "36" ); return 1;
}
void ui_message( struct ui *ui, const char *title, const char *text )
{
    (void)ui; (void)text;
    if (!strcmp( title, "数值无效" )) invalid++;
    else { fprintf( stderr, "Unexpected message: %s: %s\n", title, text ); assert( 0 ); }
}
enum ui_action ui_list_run( struct ui *ui, struct ui_list *list, const char *title, const char *context,
                            const struct ui_row *rows, int count, int can_reset )
{
    (void)ui; (void)context; assert( can_reset );
    if (!strcmp( title, "金手指" ))
    {
        assert( count == 4 && rows[3].disabled && strstr( rows[3].label, "尚未接入" ) );
        switch (main_step++)
        {
        case 0: assert( !rows[0].on ); list->selection = 0; return UI_ACTION_CHOOSE;
        case 1: assert( rows[0].on ); list->selection = 2; return UI_ACTION_CHOOSE;
        case 2: assert( strstr( rows[2].value, "开 · 35" ) ); return UI_ACTION_BACK;
        case 3: assert( rows[0].on && strstr( rows[2].value, "35" ) ); list->selection = 0; return UI_ACTION_RESET;
        case 4: assert( !rows[0].on && strstr( rows[2].value, "开 · 35" ) ); return UI_ACTION_BACK;
        default: assert( 0 );
        }
    }
    assert( !strcmp( title, "测试数值" ) && count == 3 );
    switch (detail_step++)
    {
    case 0: assert( !rows[0].on ); list->selection = 0; return UI_ACTION_CHOOSE;
    case 1: assert( rows[0].on && !strcmp( rows[1].value, "10" ) ); list->selection = 1; return UI_ACTION_RIGHT;
    case 2: assert( !strcmp( rows[1].value, "15" ) ); return UI_ACTION_LEFT;
    case 3: assert( !strcmp( rows[1].value, "10" ) ); return UI_ACTION_CHOOSE;
    case 4: assert( invalid == 1 && !strcmp( rows[1].value, "10" ) ); return UI_ACTION_CHOOSE;
    case 5: assert( !strcmp( rows[1].value, "35" ) ); return UI_ACTION_RESET;
    case 6: assert( !strcmp( rows[1].value, "10" ) ); return UI_ACTION_CHOOSE;
    case 7: assert( !strcmp( rows[1].value, "35" ) ); return UI_ACTION_BACK;
    default: assert( 0 );
    }
    return UI_ACTION_BACK;
}
int main( int argc, char **argv )
{
    assert( argc == 3 ); root = argv[1];
    char exe[768], keys[768]; snprintf( exe, sizeof(exe), "%s/Cheats.exe", root );
    assert( launcher_program_settings_path( root, exe, settings, sizeof(settings) ) && launcher_keys_path( exe, keys, sizeof(keys) ) );
    struct game_profile_catalog *catalog = calloc( 1, sizeof(*catalog) );
    assert( catalog && game_profiles_load( argv[2], catalog ) == GAME_PROFILE_OK );
    int preserved;
    assert( game_profile_apply( settings, keys, &catalog->entries[0], "", "", &preserved ) == GAME_PROFILE_OK );
    struct ui ui = {0}; ui.running = 1;
    launcher_cheats_open( &ui, root, exe, "Test game" );
    launcher_cheats_open( &ui, root, exe, "Test game" );
    assert( main_step == 5 && detail_step == 8 && invalid == 1 );
    struct game_cheats defs; struct launcher_kv state;
    assert( game_profile_cheats_read( settings, &defs, &state ) == GAME_PROFILE_OK );
    assert( !game_cheats_enabled( &state ) && game_cheat_enabled( &state, &defs.entries[1] ) && game_cheat_value( &state, &defs.entries[1] ) == 35 );
    game_profiles_clear( catalog ); free( catalog );
    puts( "cheat menu: master and item toggles, step/prompt/range validation/reset, persisted reopen passed" );
}
