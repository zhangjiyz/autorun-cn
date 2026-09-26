/* Upgrade old default-off settings once; never re-enable features after a
 * user turns them off and restarts. Keep unrelated settings intact. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../source/upstream_feature_defaults.h"

static const char *const features[] = {
    "floating-keyboard-enabled", "keyboard-on-text-focus",
    "setup-components-before-games", "read-game-dxvk-conf"
};

int main(void)
{
    struct wine_nx_config config, disk, full, before;
    char path[] = "/tmp/wine-nx-feature-defaults.XXXXXX", key[32];
    char text[32];
    unsigned int i;
    int fd;

    assert( wine_nx_config_parse( &config, "{\"floating-keyboard-enabled\":false,"
        "\"keyboard-on-text-focus\":false,\"setup-components-before-games\":false,"
        "\"read-game-dxvk-conf\":false,\"sd-cn-strategy\":false,\"future\":\"keep\"}" ) );
    assert( wine_nx_upstream_feature_defaults( &config ) == 1 );
    for (i = 0; i < sizeof(features) / sizeof(features[0]); i++)
        assert( wine_nx_config_bool( &config, features[i], 0 ) );
    assert( !wine_nx_config_bool( &config, "sd-cn-strategy", 1 ) );
    assert( wine_nx_config_string_value( &config, "future", text, sizeof(text) ) );
    assert( !strcmp( text, "keep" ) );

    fd = mkstemp( path );
    assert( fd >= 0 && !close( fd ) );
    for (i = 0; i < sizeof(features) / sizeof(features[0]); i++)
        assert( wine_nx_config_set_bool( &config, features[i], 0 ) );
    assert( wine_nx_config_save( &config, path ) );
    assert( wine_nx_config_load( &disk, path ) );
    assert( !wine_nx_upstream_feature_defaults( &disk ) );
    for (i = 0; i < sizeof(features) / sizeof(features[0]); i++)
        assert( !wine_nx_config_bool( &disk, features[i], 1 ) );
    assert( !unlink( path ) );

    assert( wine_nx_config_parse( &config, "{}" ) );
    assert( wine_nx_upstream_feature_defaults( &config ) == 1 );
    assert( !wine_nx_upstream_feature_defaults( &config ) );
    assert( wine_nx_config_parse( &full, "{}" ) );
    for (i = 0; i < WINE_NX_CONFIG_KEYS; i++)
    {
        snprintf( key, sizeof(key), "existing-%u", i );
        assert( wine_nx_config_set_bool( &full, key, 0 ) );
    }
    before = full;
    assert( wine_nx_upstream_feature_defaults( &full ) == -1 );
    assert( !memcmp( &full, &before, sizeof(full) ) );
    puts( "Upstream features: old defaults upgraded, user opt-outs survive restart, unrelated settings preserved, full config unchanged: ok" );
    return 0;
}
