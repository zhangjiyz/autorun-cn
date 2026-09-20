#include "launcher_profiles.h"
#include "launcher.h"
#include "launcher_ui.h"
#include "launcher_list.h"
#include "autorun_update.h"
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define INDEX_LIMIT (1024u * 1024u)
struct profile_config { char url[512]; int automatic; };
struct profile_fetch
{
    const char *root, *url;
    const struct game_profile *selected;
    struct game_profile_catalog *catalog;
    int package, automatic, fallback, cached;
    Uint32 started;
    SDL_mutex *mutex;
    SDL_atomic_t done, cancel;
    unsigned long long current, total;
    enum autorun_update_result network;
    enum game_profile_result result;
};

static int profile_paths( const char *root, const char *exe, char *settings, char *keys )
{
    return launcher_program_settings_path( root, exe, settings, 768 ) && launcher_keys_path( exe, keys, 768 );
}
enum game_profile_result launcher_profiles_recover( const char *root, const char *exe )
{
    char settings[768], keys[768];
    if (!profile_paths( root, exe, settings, keys )) return GAME_PROFILE_OK;
    return game_profile_recover( settings, keys );
}

static int config_load( const char *root, struct profile_config *config )
{
    char path[768]; struct launcher_kv kv;
    if (snprintf( path, sizeof(path), "%s/profile-updates.txt", root ) >= (int)sizeof(path) || !launcher_kv_load( &kv, path )) return 0;
    strcpy( config->url, AUTORUN_PROFILE_INDEX_URL ); config->automatic = 1;
    if (!launcher_kv_get( &kv, "index-url", config->url, sizeof(config->url) ))
    {
        /* Carry forward v21-v23's explicit offline/custom repository choice. */
        char repo[204], tag[101] = "";
        if (launcher_kv_get( &kv, "repository", repo, sizeof(repo) ))
        {
            struct autorun_update_source valid;
            launcher_kv_get( &kv, "tag", tag, sizeof(tag) );
            if (!repo[0]) config->url[0] = 0;
            else if (autorun_profile_source( &valid, repo, tag ))
                snprintf( config->url, sizeof(config->url), "https://github.com/%s/releases/%s%s/autorun-profiles.tsv",
                          repo, tag[0] ? "download/" : "latest/download", tag );
            else return 0;
        }
    }
    config->automatic = launcher_kv_get_int( &kv, "auto-update", 1 ) != 0;
    return !config->url[0] || autorun_https_url( config->url );
}
static int config_save( const char *root, const struct profile_config *config )
{
    char path[768]; struct launcher_kv kv;
    if (snprintf( path, sizeof(path), "%s/profile-updates.txt", root ) >= (int)sizeof(path) || !launcher_kv_load( &kv, path )) return 0;
    return launcher_kv_set( &kv, "index-url", config->url ) &&
           launcher_kv_set( &kv, "auto-update", config->automatic ? "1" : "0" ) && launcher_kv_save( &kv, path );
}

static char *read_text( const char *path )
{
    struct stat st;
    if (lstat( path, &st ) || !S_ISREG( st.st_mode) || st.st_size <= 0 || st.st_size > INDEX_LIMIT + 768) return NULL;
    FILE *file = fopen( path, "rb" );
    char *text = malloc( st.st_size + 1 );
    if (!file || !text) { if (file) fclose( file ); free( text ); return NULL; }
    size_t n = fread( text, 1, st.st_size, file );
    int ok = n == (size_t)st.st_size && fgetc( file ) == EOF && !ferror( file ) && !memchr( text, 0, n );
    fclose( file ); if (!ok) { free( text ); return NULL; } text[n] = 0; return text;
}
static int cached_index( struct profile_fetch *f )
{
    char path[768], *text = NULL, *body;
    int ok = 0;
    snprintf( path, sizeof(path), "%s/profiles-index.cache", f->root );
    if (f->url[0] && (text = read_text( path )))
    {
        body = strchr( text, '\n' );
        if (body) { *body++ = 0; if (!strcmp( text, f->url )) ok = game_profile_index_parse( body, f->catalog ); }
        free( text );
    }
    if (!ok && (!f->url[0] || !strcmp( f->url, AUTORUN_PROFILE_INDEX_URL )))
    {
        snprintf( path, sizeof(path), "%s/profiles/autorun-profiles.tsv", f->root );
        if ((text = read_text( path ))) { ok = game_profile_index_parse( text, f->catalog ); free( text ); }
    }
    return ok;
}
static int cache_index( const struct profile_fetch *f, const char *text, size_t size )
{
    char path[768], temp[800]; struct stat st;
    snprintf( path, sizeof(path), "%s/profiles-index.cache", f->root );
    snprintf( temp, sizeof(temp), "%s.new", path );
    if ((!lstat( path, &st ) && !S_ISREG( st.st_mode )) || (!lstat( temp, &st ) && !S_ISREG( st.st_mode ))) return 0;
    FILE *file = fopen( temp, "wb" );
    if (!file) return 0;
    int ok = fprintf( file, "%s\n", f->url ) > 0 && fwrite( text, 1, size, file ) == size;
    if (fflush( file ) || fsync( fileno( file ) )) ok = 0;
    if (fclose( file )) ok = 0;
    if (ok) ok = (!remove( path ) || errno == ENOENT) && !rename( temp, path );
    remove( temp ); return ok;
}
static int fetch_progress( void *opaque, unsigned long long current, unsigned long long total )
{
    struct profile_fetch *f = opaque;
    SDL_LockMutex( f->mutex ); f->current = current; f->total = total; SDL_UnlockMutex( f->mutex );
    if (f->automatic && SDL_GetTicks() - f->started >= 12000) SDL_AtomicSet( &f->cancel, 1 );
    return SDL_AtomicGet( &f->cancel );
}
static int fetch_worker( void *opaque )
{
    struct profile_fetch *f = opaque;
    f->result = GAME_PROFILE_IO; f->network = AUTORUN_UPDATE_OK;
    if (!f->package)
    {
        char *text = NULL; size_t size = 0;
        if (f->url[0])
        {
            f->network = autorun_update_text( f->url, &text, &size, f->automatic ? 5 : 30, fetch_progress, f );
            if (f->network == AUTORUN_UPDATE_OK)
            {
                f->result = game_profile_index_parse( text, f->catalog ) ? GAME_PROFILE_OK : GAME_PROFILE_INVALID;
                if (f->result == GAME_PROFILE_OK && !cache_index( f, text, size )) f->result = GAME_PROFILE_IO;
            }
        }
        if (f->result != GAME_PROFILE_OK && (!f->url[0] || f->fallback) && !SDL_AtomicGet( &f->cancel ) && cached_index( f ))
        { f->result = GAME_PROFILE_OK; f->cached = !!f->url[0]; }
        free( text );
    }
    else
    {
        char archive[768], name[128];
        const struct game_profile *p = f->selected;
        game_profile_archive_name( p, name, sizeof(name) );
        snprintf( archive, sizeof(archive), "%s/profiles/%s", f->root, name );
        /* An exact bundled hash can satisfy the request without any download. */
        if (!autorun_file_matches( archive, p->archive_size, p->digest ))
        {
            if (f->url[0]) f->network = autorun_update_file( p->url, p->archive_size, p->digest, f->root,
                archive, sizeof(archive), f->automatic ? 10 : 300, fetch_progress, f );
            else f->network = AUTORUN_UPDATE_NOT_FOUND;
        }
        if (f->network == AUTORUN_UPDATE_OK && !SDL_AtomicGet( &f->cancel ))
        {
            f->result = game_profiles_load( archive, f->catalog );
            if (f->result == GAME_PROFILE_OK && (f->catalog->count != 1 || strcmp( f->catalog->entries[0].id, p->id ) ||
                f->catalog->entries[0].version != p->version || f->catalog->entries[0].min_api != p->min_api))
                f->result = GAME_PROFILE_INVALID;
        }
    }
    SDL_AtomicSet( &f->done, 1 ); return 0;
}

static int fetch( struct ui *ui, struct profile_fetch *f )
{
    SDL_Thread *thread;
    if (!(f->mutex = SDL_CreateMutex())) return 0;
    if (!ui) { fetch_worker( f ); }
    else
    {
        thread = SDL_CreateThreadWithStackSize( fetch_worker, "game-profile", 1024 * 1024, f );
        if (!thread) { SDL_DestroyMutex( f->mutex ); f->mutex = NULL; return 0; }
        ui_progress_begin( ui );
        while (!SDL_AtomicGet( &f->done ))
        {
            struct ui_input input; char text[192]; unsigned long long current, total;
            if (!ui_begin_frame( ui )) SDL_AtomicSet( &f->cancel, 1 );
            while (ui_poll( ui, &input )) if (input.button == UI_B) SDL_AtomicSet( &f->cancel, 1 );
            SDL_LockMutex( f->mutex ); current = f->current; total = f->total; SDL_UnlockMutex( f->mutex );
            ui_background( ui );
            ui_header( ui, f->automatic ? "启动前检查适配包" : "适配包管理", f->package ? f->selected->name : "更新管理表" );
            snprintf( text, sizeof(text), "%s", f->package ? "仅下载此游戏的适配包…" : "正在读取适配包管理表…" );
            ui_text_centered( ui, ui->normal, ui->width / 2, ui->height / 2 - 30, text, ui->value );
            if (total) snprintf( text, sizeof(text), "%.0f%% · B %s", (double)current * 100 / total, f->automatic ? "跳过更新，继续启动" : "取消" );
            else snprintf( text, sizeof(text), "B %s", f->automatic ? "跳过更新，继续启动" : "取消" );
            ui_text_centered( ui, ui->small, ui->width / 2, ui->height / 2 + 30, text, ui->dim );
            ui_present( ui ); SDL_Delay( 16 );
        }
        SDL_WaitThread( thread, NULL ); ui_progress_end( ui );
    }
    SDL_DestroyMutex( f->mutex ); f->mutex = NULL;
    if (SDL_AtomicGet( &f->cancel ) || (ui && !ui->running)) return 0;
    if (f->result == GAME_PROFILE_OK) return 1;
    if (ui && !f->automatic) ui_message( ui, f->package ? "适配包未下载" : "管理表未更新",
        f->network != AUTORUN_UPDATE_OK ? "下载失败或校验不符，请检查管理表地址、附件与网络。原游戏配置保持不变。" : game_profile_error( f->result ) );
    return 0;
}
static int choose_profile( struct ui *ui, const struct game_profile_catalog *catalog, const char *title )
{
    struct ui_list list = {0};
    struct ui_row *rows = calloc( GAME_PROFILE_MAX + 2, sizeof(*rows) );
    int ids[GAME_PROFILE_MAX + 2], chosen = -1;
    char query[128] = "";
    if (!rows) return -1;
    for (;;)
    {
        int count = 1;
        memset( rows, 0, (GAME_PROFILE_MAX + 2) * sizeof(*rows) );
        snprintf( rows[0].label, sizeof(rows[0].label), "筛选名称 / 关键词" );
        snprintf( rows[0].value, sizeof(rows[0].value), "%s", query[0] ? query : "全部适配包" );
        rows[0].kind = UI_ROW_VALUE;
        rows[0].adjustable = 1;
        rows[0].help = "输入游戏名、英文名或关键词。清空输入可显示全部；选中此行按 Y 清除筛选。";
        for (int i = 0; i < catalog->count; i++)
        {
            const struct game_profile *p = &catalog->entries[i];
            if (!game_profile_matches( p, query )) continue;
            ids[count] = i;
            snprintf( rows[count].label, sizeof(rows[count].label), "%s", p->name );
            snprintf( rows[count].value, sizeof(rows[count].value), "版本 %u%s", p->version,
                      p->min_api > GAME_PROFILE_API ? " · 请先更新主程序" : "" );
            rows[count].help = p->description;
            rows[count].disabled = p->min_api > GAME_PROFILE_API;
            count++;
        }
        if (count == 1)
        {
            snprintf( rows[count].label, sizeof(rows[count].label), "没有匹配的适配包" );
            rows[count++].disabled = 1;
        }
        enum ui_action action = ui_list_run( ui, &list, "选择适配包", title, rows, count, 1 );
        if (action == UI_ACTION_BACK || action == UI_ACTION_QUIT) break;
        if (action == UI_ACTION_RESET) { query[0] = 0; memset( &list, 0, sizeof(list) ); continue; }
        if (action != UI_ACTION_CHOOSE && !(list.selection == 0 &&
            (action == UI_ACTION_LEFT || action == UI_ACTION_RIGHT))) continue;
        if (!list.selection)
        {
            char entered[128];
            if (launcher_platform_prompt( "筛选游戏名或关键词", query, entered, sizeof(entered) ))
                snprintf( query, sizeof(query), "%s", entered );
            memset( &list, 0, sizeof(list) );
            continue;
        }
        if (!rows[list.selection].disabled) { chosen = ids[list.selection]; break; }
    }
    free( rows ); return chosen;
}


static enum game_profile_result install_selected( struct ui *ui, const char *root, const char *url,
        const char *settings, const char *keys, const struct game_profile *selected,
        const struct game_profile_binding *binding, int automatic, Uint32 started )
{
    int same = !strcmp( binding->id, selected->id ) && !strcmp( binding->repository, url ) && !binding->tag[0];
    if (same && selected->version <= binding->version) return GAME_PROFILE_OLD;
    if (selected->min_api > GAME_PROFILE_API) return GAME_PROFILE_INCOMPATIBLE;
    char message[896];
    if (!automatic)
    {
        snprintf( message, sizeof(message), "%s · 版本 %u\n%s\n\n%s", selected->name, selected->version, selected->description,
            same ? "只下载此游戏适配包；保留本地修改，并备份当前配置。" : "请确认游戏版本相符。首次应用或更换来源会应用包内默认配置、按键和封面，金手指默认关闭。原配置会先备份。" );
        if (!ui_confirm( ui, "应用此游戏适配包？", message, "应用" )) return GAME_PROFILE_IO;
    }
    struct game_profile_catalog *package = calloc( 1, sizeof(*package) );
    if (!package) return GAME_PROFILE_IO;
    struct profile_fetch task = {.root = root, .url = url, .selected = selected, .catalog = package,
        .package = 1, .automatic = automatic, .started = started};
    enum game_profile_result result = GAME_PROFILE_IO;
    int preserved;
    if (fetch( ui, &task ))
    {
        result = game_profile_apply( settings, keys, &package->entries[0], url, "", &preserved );
        if (ui && !automatic && result != GAME_PROFILE_OK) ui_message( ui, "适配包未应用", game_profile_error( result ) );
    }
    game_profiles_clear( package ); free( package );
    if (ui && !automatic && result == GAME_PROFILE_OK)
        ui_message( ui, "适配包已应用", "已应用此游戏的适配包。配置、按键、金手指和封面已保存，可恢复上次配置。" );
    return result;
}

void launcher_profiles_settings( struct ui *ui, const char *root )
{
    struct profile_config config; struct ui_list list = {0};
    if (!config_load( root, &config )) { ui_message( ui, "适配包管理", "管理表地址无效，请检查 profile-updates.txt。" ); return; }
    for (;;)
    {
        struct ui_row rows[3] = {0};
        char address_help[896];
        snprintf( rows[0].label, sizeof(rows[0].label), "管理表更新地址" );
        if (config.url[0]) snprintf( rows[0].value, sizeof(rows[0].value), "%.*s · 编辑",
            (int)strcspn( config.url + 8, "/" ), config.url + 8 );
        else snprintf( rows[0].value, sizeof(rows[0].value), "离线 · 内置管理表" );
        snprintf( address_help, sizeof(address_help), "%s\n\n输入管理表文件的 HTTPS 地址；留空使用内置表。更换地址后，已有游戏需重新绑定。", config.url );
        rows[0].help = address_help;
        snprintf( rows[1].label, sizeof(rows[1].label), "立即更新管理表" );
        rows[1].help = "只获取名称、版本和下载地址列表，不下载任何游戏适配包。";
        snprintf( rows[2].label, sizeof(rows[2].label), "启动游戏前自动更新适配包" );
        snprintf( rows[2].value, sizeof(rows[2].value), "%s", config.automatic ? "开" : "关" );
        rows[2].kind = UI_ROW_SWITCH; rows[2].on = config.automatic; rows[2].adjustable = 1;
        rows[2].help = "默认开启，仅更新已绑定的游戏。失败或取消继续使用原配置；可在游戏配置中恢复上次配置。";
        enum ui_action action = ui_list_run( ui, &list, "适配包管理", NULL, rows, 3, 0 );
        if (action == UI_ACTION_BACK || action == UI_ACTION_QUIT) return;
        if (list.selection == 0 && action == UI_ACTION_CHOOSE)
        {
            struct profile_config next = config;
            if (!launcher_platform_prompt( "管理表 HTTPS 地址（留空使用内置表）", config.url, next.url, sizeof(next.url) )) continue;
            if (next.url[0] && !autorun_https_url( next.url )) { ui_message( ui, "地址无效", "请填写完整 HTTPS 文件地址。" ); continue; }
            if (!config_save( root, &next )) ui_message( ui, "保存失败", "无法保存管理表地址。" );
            else config = next;
        }
        else if (list.selection == 1 && action == UI_ACTION_CHOOSE)
        {
            struct game_profile_catalog *catalog = calloc( 1, sizeof(*catalog) );
            if (!catalog) continue;
            struct profile_fetch task = {.root = root, .url = config.url, .catalog = catalog};
            if (fetch( ui, &task ))
            {
                char message[96]; snprintf( message, sizeof(message), "管理表已更新，共 %d 个游戏。未下载任何游戏适配包。", catalog->count );
                ui_message( ui, "管理表已更新", message );
            }
            game_profiles_clear( catalog ); free( catalog );
        }
        else if (list.selection == 2 && (action == UI_ACTION_CHOOSE || action == UI_ACTION_LEFT || action == UI_ACTION_RIGHT))
        {
            struct profile_config next = config; next.automatic = !next.automatic;
            if (config_save( root, &next )) config = next;
            else ui_message( ui, "保存失败", "无法保存自动更新设置。" );
        }
    }
}

void launcher_profiles_open( struct ui *ui, const char *root, const char *exe, const char *title )
{
    char settings[768], keys[768]; struct ui_list list = {0};
    if (!profile_paths( root, exe, settings, keys )) return;
    if (launcher_settings_on_usb( exe )) { ui_message( ui, "适配包更新", "当前适配包安装支持 SD 卡上的游戏。" ); return; }
    enum game_profile_result recovered = game_profile_recover( settings, keys );
    if (recovered != GAME_PROFILE_OK) { ui_message( ui, "适配包恢复", game_profile_error( recovered ) ); return; }
    for (;;)
    {
        struct game_profile_binding binding; struct profile_config config;
        struct ui_row rows[4] = {0};
        if (!config_load( root, &config ) || game_profile_binding_read( settings, &binding ) != GAME_PROFILE_OK)
        { ui_message( ui, "适配包更新", "无法读取管理表设置或游戏绑定。" ); return; }
        snprintf( rows[0].label, sizeof(rows[0].label), "选择 / 更换适配包" );
        snprintf( rows[0].value, sizeof(rows[0].value), "%s", binding.id[0] ? binding.name : "尚未选择" );
        rows[0].help = "按名称或关键词筛选管理表，只下载选中游戏的适配包。";
        snprintf( rows[1].label, sizeof(rows[1].label), "检查此游戏适配包更新" );
        if (binding.id[0]) snprintf( rows[1].value, sizeof(rows[1].value), "当前版本 %u", binding.version );
        rows[1].disabled = !binding.id[0];
        rows[1].help = "根据管理表中的稳定 ID 和版本，只更新此游戏。";
        snprintf( rows[2].label, sizeof(rows[2].label), "恢复上次配置" );
        char backup[800]; struct stat st; snprintf( backup, sizeof(backup), "%s.profile-backup", settings );
        rows[2].disabled = stat( backup, &st ) != 0;
        rows[2].help = "恢复上一次安装前的配置、按键、金手指、封面和绑定。";
        snprintf( rows[3].label, sizeof(rows[3].label), "管理表与自动更新设置" );
        rows[3].help = "全局只维护一个管理表地址，也可在主程序设置 → 系统中修改。";
        enum ui_action action = ui_list_run( ui, &list, "适配包更新", title, rows, 4, 0 );
        if (action == UI_ACTION_BACK || action == UI_ACTION_QUIT) return;
        if (action != UI_ACTION_CHOOSE || rows[list.selection].disabled) continue;
        if (list.selection == 3) { launcher_profiles_settings( ui, root ); continue; }
        if (list.selection == 2)
        {
            if (ui_confirm( ui, "恢复上次配置？", rows[2].help, "恢复" ))
            {
                enum game_profile_result result = game_profile_restore( settings, keys );
                ui_message( ui, "恢复上次配置", result == GAME_PROFILE_OK ? "已恢复上一次应用前的配置。" : game_profile_error( result ) );
            }
            continue;
        }
        struct game_profile_catalog *catalog = calloc( 1, sizeof(*catalog) );
        if (!catalog) continue;
        struct profile_fetch task = {.root = root, .url = config.url, .catalog = catalog, .fallback = 1};
        if (fetch( ui, &task ))
        {
            if (task.cached) ui_message( ui, "使用已保存管理表", "未能更新在线管理表，当前显示缓存或内置列表。" );
            int selected = -1;
            if (!list.selection) selected = choose_profile( ui, catalog, title );
            else for (int i = 0; i < catalog->count; i++) if (!strcmp( catalog->entries[i].id, binding.id )) { selected = i; break; }
            if (selected >= 0)
            {
                enum game_profile_result result = install_selected( ui, root, config.url, settings, keys, &catalog->entries[selected], &binding, 0, 0 );
                if (result == GAME_PROFILE_OLD || result == GAME_PROFILE_INCOMPATIBLE) ui_message( ui, "适配包更新", game_profile_error( result ) );
            }
            else if (list.selection) ui_message( ui, "未找到适配包", "当前管理表没有此游戏，原配置保持不变。" );
        }
        game_profiles_clear( catalog ); free( catalog );
    }
}

int launcher_profiles_before_start( struct ui *ui, const char *root, const char *exe )
{
    /* A launcher launch reaches the runtime too; check once across that handoff. */
    static char last[768]; static time_t last_check;
    char settings[768], keys[768]; struct profile_config config; struct game_profile_binding binding;
    if (launcher_settings_on_usb( exe ) || !profile_paths( root, exe, settings, keys )) return 1;
    if (game_profile_recover( settings, keys ) != GAME_PROFILE_OK) return 0;
    if (!config_load( root, &config ) || !config.automatic || game_profile_binding_read( settings, &binding ) != GAME_PROFILE_OK ||
        !binding.id[0] || strcmp( binding.repository, config.url ) || binding.tag[0]) return 1;
    Uint32 now = SDL_GetTicks();
    time_t wall_time = time( NULL );
    if (!strcmp( last, exe ) && wall_time >= last_check && wall_time - last_check < 30) return 1;
    snprintf( last, sizeof(last), "%s", exe ); last_check = wall_time;
    struct game_profile_catalog *catalog = calloc( 1, sizeof(*catalog) );
    if (!catalog) return 1;
    struct profile_fetch task = {.root = root, .url = config.url, .catalog = catalog, .automatic = 1, .fallback = 1, .started = now};
    if (fetch( ui, &task )) for (int i = 0; i < catalog->count; i++)
    {
        struct game_profile *p = &catalog->entries[i];
        if (strcmp( p->id, binding.id ) || p->version <= binding.version || p->min_api > GAME_PROFILE_API) continue;
        enum game_profile_result result = install_selected( ui, root, config.url, settings, keys, p, &binding, 1, now );
        fprintf( stderr, "[PROFILE] auto-update id=%s version=%u result=%d\n", p->id, p->version, result );
        break;
    }
    game_profiles_clear( catalog ); free( catalog );
    return (!ui || ui->running) && game_profile_recover( settings, keys ) == GAME_PROFILE_OK;
}
