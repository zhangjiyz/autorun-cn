#include "game_directories.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

static int valid_relative( const char *relative )
{
    const char *part;

    if (!relative || !relative[0] || strlen( relative ) >= 256) return 0;
    for (part = relative; *part; )
    {
        const char *end = strchr( part, '/' );
        size_t size = end ? (size_t)(end - part) : strlen( part );

        if (!size || (size == 1 && part[0] == '.') ||
            (size == 2 && part[0] == '.' && part[1] == '.') ||
            part[size - 1] == '.' || part[size - 1] == ' ' ||
            memchr( part, '\\', size ) || memchr( part, ':', size )) return 0;
        for (size_t i = 0; i < size; i++) if ((unsigned char)part[i] < 32) return 0;
        if (!end) return 1;
        part = end + 1;
    }
    return 0;
}

int game_directory_ensure( const char *executable, const char *relative, char *resolved, size_t capacity )
{
    const char *slash, *part;
    size_t length;
    struct stat st;

    if (!executable || !resolved || !valid_relative( relative ) ||
        !(slash = strrchr( executable, '/' )))
    {
        errno = EINVAL;
        return 0;
    }
    length = (size_t)(slash - executable);
    if (!length || length >= capacity)
    {
        errno = ENAMETOOLONG;
        return 0;
    }
    memcpy( resolved, executable, length );
    resolved[length] = 0;
    if (lstat( resolved, &st )) return 0;
    if (!S_ISDIR( st.st_mode ) || S_ISLNK( st.st_mode ))
    {
        errno = ENOTDIR;
        return 0;
    }

    for (part = relative; *part; )
    {
        const char *end = strchr( part, '/' );
        size_t size = end ? (size_t)(end - part) : strlen( part );

        if (length + 1 + size >= capacity)
        {
            errno = ENAMETOOLONG;
            return 0;
        }
        resolved[length++] = '/';
        memcpy( resolved + length, part, size );
        length += size;
        resolved[length] = 0;
        if (lstat( resolved, &st ))
        {
            if (errno != ENOENT || mkdir( resolved, 0777 ) || lstat( resolved, &st )) return 0;
        }
        if (!S_ISDIR( st.st_mode ) || S_ISLNK( st.st_mode ))
        {
            errno = ENOTDIR;
            return 0;
        }
        if (!end) break;
        part = end + 1;
    }
    return 1;
}
