#ifndef LAUNCHER_PROFILES_H
#define LAUNCHER_PROFILES_H
#include "game_profiles.h"
struct ui;
void launcher_profiles_settings( struct ui *ui, const char *runtime_dir );
int launcher_profiles_before_start( struct ui *ui, const char *runtime_dir, const char *exe );
void launcher_profiles_open( struct ui *ui, const char *runtime_dir, const char *exe, const char *title );
enum game_profile_result launcher_profiles_recover( const char *runtime_dir, const char *exe );
#endif
