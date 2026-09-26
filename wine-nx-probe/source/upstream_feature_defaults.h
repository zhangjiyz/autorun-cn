/* Apply the requested upstream test defaults once. Later user choices stay
 * authoritative, including turning every feature back off. */
#ifndef WINE_NX_UPSTREAM_FEATURE_DEFAULTS_H
#define WINE_NX_UPSTREAM_FEATURE_DEFAULTS_H

#include "config_json.h"

#define WINE_NX_UPSTREAM_FEATURE_DEFAULTS "upstream-feature-defaults-v1"

static inline int wine_nx_upstream_feature_defaults( struct wine_nx_config *config )
{
    struct wine_nx_config next;

    if (wine_nx_config_bool( config, WINE_NX_UPSTREAM_FEATURE_DEFAULTS, 0 )) return 0;
    next = *config;
    if (!wine_nx_config_set_bool( &next, "floating-keyboard-enabled", 1 ) ||
        !wine_nx_config_set_bool( &next, "keyboard-on-text-focus", 1 ) ||
        !wine_nx_config_set_bool( &next, "setup-components-before-games", 1 ) ||
        !wine_nx_config_set_bool( &next, "read-game-dxvk-conf", 1 ) ||
        !wine_nx_config_set_bool( &next, WINE_NX_UPSTREAM_FEATURE_DEFAULTS, 1 )) return -1;
    *config = next;
    return 1;
}

#endif
