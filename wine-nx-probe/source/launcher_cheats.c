#include "launcher_cheats.h"
#include "launcher.h"
#include "launcher_ui.h"
#include "launcher_list.h"
#include "game_profiles.h"

static int save( struct ui *ui, const char *settings, const char *keys, struct launcher_kv *state )
{
    enum game_profile_result result = game_profile_cheats_save( settings, keys, state );
    if (result == GAME_PROFILE_OK) return 1;
    ui_message( ui, "金手指设置未保存", game_profile_error( result ) );
    return 0;
}

static void edit( struct ui *ui, const char *settings, const char *keys,
                  const struct game_cheat *cheat, struct launcher_kv *state )
{
    struct ui_list list = {0};
    for (;;)
    {
        struct ui_row rows[3] = {0};
        struct launcher_kv next = *state;
        int enabled = game_cheat_enabled( state, cheat );
        int32_t value = game_cheat_value( state, cheat );
        char help[192];
        snprintf( rows[0].label, sizeof(rows[0].label), "此项开关" );
        snprintf( rows[0].value, sizeof(rows[0].value), "%s", enabled ? "开" : "关" );
        rows[0].kind = UI_ROW_SWITCH; rows[0].on = enabled; rows[0].adjustable = 1;
        rows[0].help = cheat->description;
        int count = 1;
        if (cheat->integer)
        {
            snprintf( rows[1].label, sizeof(rows[1].label), "数值" );
            snprintf( rows[1].value, sizeof(rows[1].value), "%d", (int)value );
            snprintf( help, sizeof(help), "范围 %d 至 %d，步长 %d。左右调整，A 输入，Y 恢复默认值。",
                      (int)cheat->minimum, (int)cheat->maximum, (int)cheat->step );
            rows[1].help = help; rows[1].kind = UI_ROW_VALUE; rows[1].adjustable = 1; count++;
        }
        snprintf( rows[count].label, sizeof(rows[count].label), "当前仅保存设置，游戏效果尚未接入" );
        rows[count].kind = UI_ROW_INFO; rows[count].disabled = 1;
        enum ui_action action = ui_list_run( ui, &list, cheat->name, "金手指设置", rows, count + 1, 1 );
        if (action == UI_ACTION_BACK || action == UI_ACTION_QUIT) return;
        if (list.selection >= count) continue;
        if (action != UI_ACTION_CHOOSE && action != UI_ACTION_LEFT && action != UI_ACTION_RIGHT && action != UI_ACTION_RESET) continue;
        if (!list.selection)
        {
            if (!game_cheat_set_enabled( &next, cheat, action == UI_ACTION_RESET ? 0 : !enabled )) continue;
        }
        else
        {
            int64_t changed = value;
            if (action == UI_ACTION_RESET) changed = cheat->initial;
            else if (action == UI_ACTION_LEFT) changed -= cheat->step;
            else if (action == UI_ACTION_RIGHT) changed += cheat->step;
            else
            {
                char entered[64], initial[32]; int32_t parsed;
                snprintf( initial, sizeof(initial), "%d", (int)value );
                if (!launcher_platform_prompt( "输入金手指数值", initial, entered, sizeof(entered) )) continue;
                if (!game_cheat_value_parse( cheat, entered, &parsed ))
                { ui_message( ui, "数值无效", help ); continue; }
                changed = parsed;
            }
            if (!game_cheat_value_valid( cheat, changed ) || !game_cheat_set_value( &next, cheat, changed )) continue;
        }
        if (save( ui, settings, keys, &next )) *state = next;
    }
}

void launcher_cheats_open( struct ui *ui, const char *root, const char *exe, const char *title )
{
    char settings[768], keys[768];
    struct game_cheats *definitions = NULL;
    struct ui_row *rows = NULL;
    struct launcher_kv state;
    struct ui_list list = {0};
    if (launcher_settings_on_usb( exe ))
    { ui_message( ui, "金手指", "当前适配包功能支持 SD 卡上的游戏。" ); return; }
    if (!launcher_program_settings_path( root, exe, settings, sizeof(settings) ) ||
        !launcher_keys_path( exe, keys, sizeof(keys) )) return;
    enum game_profile_result result = game_profile_recover( settings, keys );
    if (result != GAME_PROFILE_OK) { ui_message( ui, "金手指", game_profile_error( result ) ); return; }
    definitions = calloc( 1, sizeof(*definitions) ); rows = calloc( GAME_CHEATS_MAX + 2, sizeof(*rows) );
    if (!definitions || !rows) goto done;
    result = game_profile_cheats_read( settings, definitions, &state );
    if (result != GAME_PROFILE_OK) { ui_message( ui, "金手指", game_profile_error( result ) ); goto done; }
    if (!definitions->count)
    { ui_message( ui, "金手指", "此游戏的适配包尚未提供金手指条目。可先在“适配包更新”中选择或更新适配包。" ); goto done; }
    for (;;)
    {
        memset( rows, 0, (GAME_CHEATS_MAX + 2) * sizeof(*rows) );
        int enabled = game_cheats_enabled( &state );
        snprintf( rows[0].label, sizeof(rows[0].label), "金手指总开关" );
        snprintf( rows[0].value, sizeof(rows[0].value), "%s", enabled ? "开" : "关" );
        rows[0].kind = UI_ROW_SWITCH; rows[0].on = enabled; rows[0].adjustable = 1;
        rows[0].help = "仅影响此游戏。关闭总开关会保留各项设置；Y 关闭总开关。";
        for (int i = 0; i < definitions->count; i++)
        {
            const struct game_cheat *c = &definitions->entries[i];
            snprintf( rows[i + 1].label, sizeof(rows[i + 1].label), "%s", c->name );
            snprintf( rows[i + 1].value, sizeof(rows[i + 1].value), "%s", game_cheat_enabled( &state, c ) ? "开" : "关" );
            if (c->integer) snprintf( rows[i + 1].value, sizeof(rows[i + 1].value), "%s · %d",
                game_cheat_enabled( &state, c ) ? "开" : "关", (int)game_cheat_value( &state, c ) );
            rows[i + 1].help = c->description;
        }
        int info = definitions->count + 1;
        snprintf( rows[info].label, sizeof(rows[info].label), "当前仅保存设置，游戏效果尚未接入" );
        rows[info].kind = UI_ROW_INFO; rows[info].disabled = 1;
        enum ui_action action = ui_list_run( ui, &list, "金手指", title, rows, info + 1, 1 );
        if (action == UI_ACTION_BACK || action == UI_ACTION_QUIT) break;
        if (list.selection == 0 && (action == UI_ACTION_CHOOSE || action == UI_ACTION_LEFT || action == UI_ACTION_RIGHT || action == UI_ACTION_RESET))
        {
            struct launcher_kv next = state;
            if (launcher_kv_set( &next, "enabled", action == UI_ACTION_RESET || enabled ? "0" : "1" ) && save( ui, settings, keys, &next )) state = next;
        }
        else if (list.selection > 0 && list.selection < info && action == UI_ACTION_CHOOSE)
            edit( ui, settings, keys, &definitions->entries[list.selection - 1], &state );
    }
 done:
    free( rows ); free( definitions );
}
