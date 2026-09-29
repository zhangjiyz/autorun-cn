#ifndef WINE_NX_GAME_DIRECTORIES_H
#define WINE_NX_GAME_DIRECTORIES_H

#include <stddef.h>

/* Create a configured directory below the selected executable's directory. */
int game_directory_ensure( const char *executable, const char *relative, char *resolved, size_t capacity );

#endif
