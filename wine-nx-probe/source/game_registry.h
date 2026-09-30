#ifndef WINE_NX_GAME_REGISTRY_H
#define WINE_NX_GAME_REGISTRY_H

#include <stdint.h>
#include <string.h>
#include <strings.h>

/* Only a named game's HKLM Software subkey, using profile '/' separators. */
static inline int game_registry_valid_key( const char *path )
{
    const char *part;
    if (!path || strlen( path ) >= 224 || strncasecmp( path, "Software/", 9 )) return 0;
    for (part = path + 9; *part; )
    {
        const char *end = strchr( part, '/' );
        size_t size = end ? (size_t)(end - part) : strlen( part );
        if (!size || (size == 1 && part[0] == '.') || (size == 2 && !memcmp( part, "..", 2 )) ||
            part[size - 1] == '.' || part[size - 1] == ' ') return 0;
        for (size_t i = 0; i < size; i++)
            if ((unsigned char)part[i] < 32 || (unsigned char)part[i] >= 127 ||
                part[i] == '\\' || part[i] == ':') return 0;
        if (!end) return 1;
        part = end + 1;
    }
    return 0;
}

/* Native NT status; persists INSTALLDIR before the guest loader runs. */
uint32_t game_registry_ensure_install_dir( const char *key, const char *dos_executable, int *changed );

#endif
