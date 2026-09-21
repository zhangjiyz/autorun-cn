/* Drive the real menu/worker with a local release bundle and scripted UI choices. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "launcher_profiles.h"
#include "launcher_ui.h"
#include "launcher.h"
#include "autorun_update.h"

static int menu_step, filter_step, applied, latest, restored, prompts, default_source_check;
static char settings_path[768];
int ui_begin_frame( struct ui *ui ) { return ui->running; }
int ui_poll( struct ui *ui, struct ui_input *input ) { (void)ui; (void)input; return 0; }
void ui_progress_begin( struct ui *ui ) { (void)ui; }
void ui_progress_end( struct ui *ui ) { (void)ui; }
void ui_background( struct ui *ui ) { (void)ui; }
void ui_header( struct ui *ui, const char *title, const char *subtitle ) { (void)ui; (void)title; (void)subtitle; }
void ui_text_centered( struct ui *ui, TTF_Font *font, int cx, int y, const char *text, SDL_Color color )
{ (void)ui; (void)font; (void)cx; (void)y; (void)text; (void)color; }
void ui_present( struct ui *ui ) { (void)ui; }
int launcher_platform_prompt( const char *header, const char *initial, char *out, size_t size )
{
    (void)header; (void)initial;
    snprintf( out, size, "%s", prompts++ ? "新仙剑" : "nothing-matches" ); return 1;
}
void ui_message( struct ui *ui, const char *title, const char *text )
{
    (void)ui;
    if (!strcmp( title, "适配包已应用" )) applied++;
    else if (strstr( text, "最新版本" )) latest++;
    else if (!strcmp( text, "已恢复上一次应用前的配置和游戏程序。" )) restored++;
    else { fprintf( stderr, "unexpected UI message: %s: %s\n", title, text ); assert( 0 ); }
}
int ui_confirm( struct ui *ui, const char *title, const char *text, const char *yes )
{ (void)ui; (void)title; (void)text; (void)yes; return 1; }
enum ui_action ui_list_run( struct ui *ui, struct ui_list *list, const char *title, const char *context,
                            const struct ui_row *rows, int count, int can_reset )
{
    (void)ui; (void)context; (void)can_reset;
    if (default_source_check)
    {
        assert( !strcmp( title, "适配包管理" ) && count == 3 );
        assert( strstr( rows[0].help, AUTORUN_PROFILE_INDEX_URL ) );
        assert( rows[2].on );
        default_source_check = 0;
        return UI_ACTION_BACK;
    }
    if (!strcmp( title, "选择适配包" ))
    {
        switch (filter_step++)
        {
        case 0: assert( count >= 3 ); list->selection = 0; return UI_ACTION_CHOOSE;
        case 1: assert( count == 2 && rows[1].disabled && rows[0].adjustable ); list->selection = 0; return UI_ACTION_RESET;
        case 2: assert( count >= 3 ); list->selection = 0; return UI_ACTION_CHOOSE;
        case 3: assert( count == 3 && strstr( rows[1].label, "新仙剑" ) && !rows[1].disabled ); list->selection = 1; return UI_ACTION_CHOOSE;
        default: assert( 0 );
        }
    }
    assert( !strcmp( title, "适配包更新" ) && count == 4 );
    struct game_profile_binding binding;
    assert( game_profile_binding_read( settings_path, &binding ) == GAME_PROFILE_OK );
    switch (menu_step++)
    {
    case 0: assert( rows[1].disabled && !binding.id[0] ); list->selection = 0; return UI_ACTION_CHOOSE;
    case 1: assert( !rows[1].disabled && !strcmp( binding.id, "newpal-steam" ) ); list->selection = 1; return UI_ACTION_CHOOSE;
    case 2: assert( !rows[2].disabled ); list->selection = 2; return UI_ACTION_CHOOSE;
    case 3: assert( !rows[1].disabled && !strcmp( binding.id, "newpal-steam" ) ); return UI_ACTION_BACK;
    default: assert( 0 );
    }
    return UI_ACTION_BACK;
}
int main( int argc, char **argv )
{
    assert( argc == 5 );
    unsigned base_version, other_version;
    assert( sscanf( argv[3], "%u", &base_version ) == 1 && base_version > 0 );
    assert( sscanf( argv[4], "%u", &other_version ) == 1 && other_version > 0 );
    char exe[768]; snprintf( exe, sizeof(exe), "%s/Game.exe", argv[1] );
    snprintf( settings_path, sizeof(settings_path), "%s/Game.wine-nx.txt", argv[1] );
    assert( !SDL_Init( 0 ) );
    struct ui ui = {0}; ui.running = 1; ui.width = 1280; ui.height = 720;
    char config_path[768]; snprintf( config_path, sizeof(config_path), "%s/profile-updates.txt", argv[1] );
    const char *previous = !strcmp( AUTORUN_PROFILE_INDEX_URL, AUTORUN_RELEASE_PROFILE_INDEX_URL ) ?
        "https://cnb.cool/PalmMuse/autorun-cn/-/releases/download/profile-debug/autorun-profiles.tsv" :
        AUTORUN_RELEASE_PROFILE_INDEX_URL;
    FILE *file = fopen( config_path, "w" ); assert( file );
    assert( fprintf( file, "index-url=%s\nauto-update=0\nbuild-index-url=%s\n", previous, previous ) > 0 );
    assert( !fclose( file ) );
    assert( launcher_profiles_before_start( NULL, argv[1], exe ) );
    struct launcher_kv saved; assert( launcher_kv_load( &saved, config_path ) );
    char value[512];
    assert( launcher_kv_get( &saved, "index-url", value, sizeof(value) ) && !strcmp( value, AUTORUN_PROFILE_INDEX_URL ) );
    assert( launcher_kv_get( &saved, "build-index-url", value, sizeof(value) ) && !strcmp( value, AUTORUN_PROFILE_INDEX_URL ) );
    assert( launcher_kv_get_int( &saved, "auto-update", 1 ) == 0 );
    file = fopen( config_path, "w" ); assert( file );
    assert( fprintf( file, "index-url=%s\nauto-update=0\n", AUTORUN_LEGACY_GITHUB_PROFILE_INDEX_URL ) > 0 );
    assert( !fclose( file ) );
    assert( launcher_profiles_before_start( NULL, argv[1], exe ) );
    assert( launcher_kv_load( &saved, config_path ) );
    assert( launcher_kv_get( &saved, "index-url", value, sizeof(value) ) && !strcmp( value, AUTORUN_PROFILE_INDEX_URL ) );
    assert( launcher_kv_get( &saved, "build-index-url", value, sizeof(value) ) && !strcmp( value, AUTORUN_PROFILE_INDEX_URL ) );
    assert( !unlink( config_path ) );
    file = fopen( config_path, "w" ); assert( file );
    assert( fprintf( file, "index-url=https://example.invalid/custom.tsv\nauto-update=0\nbuild-index-url=%s\n", previous ) > 0 );
    assert( !fclose( file ) );
    assert( launcher_profiles_before_start( NULL, argv[1], exe ) );
    assert( launcher_kv_load( &saved, config_path ) );
    assert( launcher_kv_get( &saved, "index-url", value, sizeof(value) ) && !strcmp( value, "https://example.invalid/custom.tsv" ) );
    assert( !unlink( config_path ) );
#ifdef AUTORUN_DEBUG_BUILD
    file = fopen( config_path, "w" ); assert( file );
    assert( fprintf( file, "index-url=%s\nauto-update=0\n", AUTORUN_RELEASE_PROFILE_INDEX_URL ) > 0 );
    assert( !fclose( file ) );
    assert( launcher_profiles_before_start( NULL, argv[1], exe ) );
    assert( launcher_kv_load( &saved, config_path ) );
    assert( launcher_kv_get( &saved, "index-url", value, sizeof(value) ) && !strcmp( value, AUTORUN_PROFILE_INDEX_URL ) );
    assert( !unlink( config_path ) );
#endif
    default_source_check = 1;
    launcher_profiles_settings( &ui, argv[1] );
    assert( !default_source_check );
    file = fopen( config_path, "w" ); assert( file );
    assert( fputs( "index-url=\nauto-update=1\n", file ) >= 0 ); assert( !fclose( file ) );
    launcher_profiles_open( &ui, argv[1], exe, "Test game" );
    assert( menu_step == 4 && filter_step == 4 && applied == 2 && latest == 0 && restored == 1 );
    assert( access( settings_path, F_OK ) == 0 );
    /* Rebind offline, then provide a newer table + only this game's ZIP.
     * Automatic checks must preserve edits and never fetch another game. */
    char package_path[768], keys[768], from[768], to[768];
    snprintf( package_path, sizeof(package_path), "%s/profiles/profile-newpal-steam-v%u.zip", argv[1], base_version );
    snprintf( keys, sizeof(keys), "%s/Game.keys.txt", argv[1] );
    struct game_profile_catalog *catalog = calloc( 1, sizeof(*catalog) );
    assert( game_profiles_load( package_path, catalog ) == GAME_PROFILE_OK );
    int preserved;
    assert( game_profile_apply( settings_path, keys, &catalog->entries[0], "", "", &preserved ) == GAME_PROFILE_OK );
    struct launcher_kv options; assert( launcher_kv_load( &options, settings_path ) );
    assert( launcher_kv_set( &options, "window-fit", "0" ) && launcher_kv_save( &options, settings_path ) );
    char updated_package[64];
    snprintf( updated_package, sizeof(updated_package), "profile-newpal-steam-v%u.zip", base_version + 1 );
    const char *files[] = {"autorun-profiles.tsv", updated_package};
    for (int i = 0; i < 2; i++)
    {
        snprintf( from, sizeof(from), "%s/%s", argv[2], files[i] );
        snprintf( to, sizeof(to), "%s/profiles/%s", argv[1], files[i] );
        FILE *src = fopen( from, "rb" ), *dst = fopen( to, "wb" ); assert( src && dst );
        char buffer[4096]; size_t n;
        while ((n = fread( buffer, 1, sizeof(buffer), src ))) assert( fwrite( buffer, 1, n, dst ) == n );
        fclose( src ); fclose( dst );
    }
    snprintf( to, sizeof(to), "%s/profiles/profile-zhaoyunzhuan-v%u.zip", argv[1], other_version ); assert( !unlink( to ) );
    assert( launcher_profiles_before_start( NULL, argv[1], exe ) );
    struct game_profile_binding binding;
    assert( game_profile_binding_read( settings_path, &binding ) == GAME_PROFILE_OK && binding.version == base_version + 1 );
    assert( launcher_kv_load( &options, settings_path ) && launcher_kv_get_int( &options, "window-fit", -1 ) == 0 );
    /* With automatic updates disabled, a second game's version stays unchanged. */
    char second[768], second_settings[768], second_keys[768];
    snprintf( second, sizeof(second), "%s/Second.exe", argv[1] );
    snprintf( second_settings, sizeof(second_settings), "%s/Second.wine-nx.txt", argv[1] );
    snprintf( second_keys, sizeof(second_keys), "%s/Second.keys.txt", argv[1] );
    assert( game_profile_apply( second_settings, second_keys, &catalog->entries[0], "", "", &preserved ) == GAME_PROFILE_OK );
    file = fopen( config_path, "w" ); assert( file ); fputs( "index-url=\nauto-update=0\n", file ); fclose( file );
    assert( launcher_profiles_before_start( NULL, argv[1], second ) );
    assert( game_profile_binding_read( second_settings, &binding ) == GAME_PROFILE_OK && binding.version == base_version );
    /* A failed/missing new archive allows launch using the installed version. */
    file = fopen( config_path, "w" ); assert( file ); fputs( "index-url=\nauto-update=1\n", file ); fclose( file );
    snprintf( to, sizeof(to), "%s/profiles/%s", argv[1], updated_package ); assert( !unlink( to ) );
    assert( launcher_profiles_before_start( NULL, argv[1], second ) );
    assert( game_profile_binding_read( second_settings, &binding ) == GAME_PROFILE_OK && binding.version == base_version );
    game_profiles_clear( catalog ); free( catalog );
    SDL_Quit();
    puts( "profile menu: no-match/clear/Chinese filter, manual selection, persisted binding, update and restore passed" );
    return 0;
}
