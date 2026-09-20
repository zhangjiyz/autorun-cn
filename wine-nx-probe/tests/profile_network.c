/* Controlled transport, real catalog/package selection, install and launch policy. */
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include "launcher.h"
#include "launcher_ui.h"
#include "launcher_profiles.h"
#include "autorun_update.h"
static char index_path[768], package_path[768], source_url[512] = "https://example.invalid/catalog.tsv";
static int table_requests, package_requests, mode, menu_step, messages;
static char *contents( const char *path, size_t *size )
{
    FILE *file = fopen( path, "rb" ); assert( file ); fseek( file, 0, SEEK_END ); *size = ftell( file ); rewind( file );
    char *text = malloc( *size + 1 ); assert( text && fread( text, 1, *size, file ) == *size );
    fclose( file ); text[*size] = 0; return text;
}
enum autorun_update_result __wrap_autorun_update_text( const char *url, char **text, size_t *size,
        long timeout, autorun_update_progress progress, void *opaque )
{
    (void)progress; (void)opaque; assert( timeout <= 30 && !strcmp( url, source_url ) ); table_requests++;
    *text = NULL; *size = 0;
    if (mode == 1) return AUTORUN_UPDATE_NETWORK;
    if (mode == 2) return AUTORUN_UPDATE_CANCELLED;
    *text = contents( index_path, size ); return AUTORUN_UPDATE_OK;
}
enum autorun_update_result __wrap_autorun_update_file( const char *url, unsigned long long size, const char *digest,
        const char *root, char *path, size_t capacity, long timeout, autorun_update_progress progress, void *opaque )
{
    (void)root; (void)progress; (void)opaque;
    assert( strstr( url, "/profile-newpal-v3.zip" ) && timeout <= 10 ); package_requests++;
    if (mode == 3) return AUTORUN_UPDATE_HASH;
    assert( autorun_file_matches( package_path, size, digest ) );
    snprintf( path, capacity, "%s", package_path ); return AUTORUN_UPDATE_OK;
}
int ui_begin_frame( struct ui *ui ) { return ui->running; }
int ui_poll( struct ui *ui, struct ui_input *input ) { (void)ui; (void)input; return 0; }
void ui_progress_begin( struct ui *ui ) { (void)ui; }
void ui_progress_end( struct ui *ui ) { (void)ui; }
void ui_background( struct ui *ui ) { (void)ui; }
void ui_header( struct ui *ui, const char *title, const char *subtitle ) { (void)ui; (void)title; (void)subtitle; }
void ui_text_centered( struct ui *ui, TTF_Font *font, int x, int y, const char *text, SDL_Color color )
{ (void)ui; (void)font; (void)x; (void)y; (void)text; (void)color; }
void ui_present( struct ui *ui ) { (void)ui; }
int launcher_platform_prompt( const char *header, const char *initial, char *out, size_t size )
{ (void)header; assert( !strcmp( initial, AUTORUN_PROFILE_INDEX_URL ) ); snprintf( out, size, "%s", source_url ); return 1; }
int ui_confirm( struct ui *ui, const char *title, const char *text, const char *yes )
{ (void)ui; (void)title; (void)text; (void)yes; assert( 0 ); return 0; }
void ui_message( struct ui *ui, const char *title, const char *text )
{ (void)ui; assert( !strcmp( title, "管理表已更新" ) && strstr( text, "未下载" ) ); messages++; }
enum ui_action ui_list_run( struct ui *ui, struct ui_list *list, const char *title, const char *context,
                           const struct ui_row *rows, int count, int reset )
{
    (void)ui; (void)context; (void)reset;
    assert( !strcmp( title, "适配包管理" ) && count == 3 );
    switch (menu_step++)
    {
    case 0: assert( rows[2].on ); list->selection = 0; return UI_ACTION_CHOOSE;
    case 1: assert( strstr( rows[0].help, source_url ) ); list->selection = 1; return UI_ACTION_CHOOSE;
    case 2: assert( messages == 1 && table_requests == 1 && package_requests == 0 ); list->selection = 2; return UI_ACTION_CHOOSE;
    case 3: assert( !rows[2].on ); return UI_ACTION_BACK;
    default: assert( 0 );
    }
    return UI_ACTION_BACK;
}
static void config( const char *root, int automatic )
{
    char path[768]; snprintf( path, sizeof(path), "%s/profile-updates.txt", root );
    FILE *file = fopen( path, "w" ); assert( file );
    fprintf( file, "index-url=%s\nauto-update=%d\n", source_url, automatic ); assert( !fclose( file ) );
}
static void launch( const char *root, const struct game_profile *profile, const char *name, unsigned expected )
{
    char exe[768], settings[768], keys[768];
    snprintf( exe, sizeof(exe), "%s/%s.exe", root, name );
    snprintf( settings, sizeof(settings), "%s/%s.wine-nx.txt", root, name );
    snprintf( keys, sizeof(keys), "%s/%s.keys.txt", root, name );
    int preserved;
    assert( game_profile_apply( settings, keys, profile, source_url, "", &preserved ) == GAME_PROFILE_OK );
    assert( launcher_profiles_before_start( NULL, root, exe ) );
    struct game_profile_binding binding;
    assert( game_profile_binding_read( settings, &binding ) == GAME_PROFILE_OK && binding.version == expected );
}
int main( int argc, char **argv )
{
    assert( argc == 4 ); char root[768]; snprintf( root, sizeof(root), "%s/network", argv[1] ); assert( !mkdir( root, 0700 ) );
    snprintf( index_path, sizeof(index_path), "%s/autorun-profiles.tsv", argv[3] );
    snprintf( package_path, sizeof(package_path), "%s/profile-newpal-v3.zip", argv[3] );
    /* Index validation is separate from ZIP validation. */
    size_t length; char *valid = contents( index_path, &length );
    struct game_profile_catalog *index = calloc( 1, sizeof(*index) );
    assert( game_profile_index_parse( valid, index ) && index->count == 2 );
    assert( !strcmp( index->entries[0].id, "newpal" ) && index->entries[0].version == 3 );
    char *bad = malloc( length * 2 + 1 ); assert( bad );
    strcpy( bad, valid ); strstr( bad, "https://" )[4] = 'x'; assert( !game_profile_index_parse( bad, index ) );
    strcpy( bad, valid ); bad[length - 1] = 0; assert( !game_profile_index_parse( bad, index ) );
    strcpy( bad, valid ); char *first = strchr( valid, '\n' ) + 1, *end = strchr( first, '\n' );
    memcpy( bad + length, first, end - first + 1 ); bad[length + end - first + 1] = 0;
    assert( !game_profile_index_parse( bad, index ) );
    free( valid ); free( bad ); game_profiles_clear( index ); free( index );
    assert( !SDL_Init( 0 ) );
    struct ui ui = {0}; ui.running = 1;
    launcher_profiles_settings( &ui, root );
    struct game_profile_catalog *catalog = calloc( 1, sizeof(*catalog) );
    assert( game_profiles_load( argv[2], catalog ) == GAME_PROFILE_OK );
    launch( root, &catalog->entries[0], "Disabled", 2 ); assert( table_requests == 1 && !package_requests );
    config( root, 1 ); launch( root, &catalog->entries[0], "Enabled", 3 ); assert( table_requests == 2 && package_requests == 1 );
    mode = 3; launch( root, &catalog->entries[0], "HashFailure", 2 ); assert( package_requests == 2 );
    /* A different source cannot consume the previous source's valid cache. */
    strcpy( source_url, "https://other.invalid/table.tsv" ); config( root, 1 );
    mode = 1; launch( root, &catalog->entries[0], "Offline", 2 ); assert( package_requests == 2 );
    mode = 2; launch( root, &catalog->entries[0], "Cancelled", 2 ); assert( package_requests == 2 );
    game_profiles_clear( catalog ); free( catalog ); SDL_Quit();
    puts( "profile network: global address edit/table-only refresh, one-game auto download, disabled, hash failure, source isolation and cancellation passed" );
}
