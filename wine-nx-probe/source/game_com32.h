#ifndef GAME_COM32_H
#define GAME_COM32_H

/* A basename only: never let a profile choose a path for the registration helper. */
static inline int game_com32_valid_name( const char *name )
{
    unsigned int i = 0;
    if (!name) return 0;
    while (name[i] && i < 48)
    {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return 0;
        i++;
    }
    if (i < 5 || i > 48 || name[i] || name[i - 4] != '.' || name[i - 3] != 'd' ||
        name[i - 2] != 'l' || name[i - 1] != 'l') return 0;
    for (unsigned int j = 0; j < i - 4; j++) if (name[j] == '.') return 0;
    return 1;
}

#endif
