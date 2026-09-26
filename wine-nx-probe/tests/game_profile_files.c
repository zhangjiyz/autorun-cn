#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

static int test_rename( const char *, const char * );
#define rename test_rename
#include "../source/game_profiles.c"
#undef rename

static int crash_after, rename_count, fail_after;
static int test_rename( const char *from, const char *to )
{
    rename_count++;
    if (rename_count == fail_after) { errno = EIO; return -1; }
    int result = rename( from, to );
    if (!result && rename_count == crash_after) _exit( 73 );
    return result;
}

static void content( const char *path, const void *expected, size_t length )
{
    FILE *file = fopen( path, "rb" ); assert( file );
    unsigned char data[128];
    size_t size = fread( data, 1, sizeof(data), file );
    assert( feof( file ) && !ferror( file ) && !fclose( file ) );
    assert( size == length && !memcmp( data, expected, size ) );
}

struct fixture { char settings[768], keys[768], game[768], user[768], nested[768], empty[768]; };
static void fixture( struct fixture *f )
{
    char directory[] = "/tmp/autorun-file-test-XXXXXX", root[PATH_MAX], drive[768], game[768];
    assert( mkdtemp( directory ) && realpath( directory, root ) );
    snprintf( drive, sizeof(drive), "%s/drive_c", root ); assert( !mkdir( drive, 0700 ) );
    snprintf( game, sizeof(game), "%s/Game", drive ); assert( !mkdir( game, 0700 ) );
    snprintf( f->settings, sizeof(f->settings), "%s/Game.wine-nx.txt", game );
    snprintf( f->keys, sizeof(f->keys), "%s/Game.keys.txt", game );
    snprintf( f->game, sizeof(f->game), "%s/config.ini", game );
    snprintf( f->user, sizeof(f->user), "%s/users/steamuser/AppData/Local/CAPCOM/TEST/config.ini", drive );
    snprintf( f->nested, sizeof(f->nested), "%s/nested/new.bin", game );
    snprintf( f->empty, sizeof(f->empty), "%s/empty.ini", game );
    assert( replacement_parents( f->user, 1 ) );
    assert( durable_write( f->game, "old\0game", 8 ) && durable_write( f->user, "old user\r\n", 10 ) );
}

static void original( const struct fixture *f )
{
    content( f->game, "old\0game", 8 ); content( f->user, "old user\r\n", 10 );
    struct stat st;
    assert( lstat( f->nested, &st ) && errno == ENOENT );
    assert( lstat( f->empty, &st ) && errno == ENOENT );
}

int main( int argc, char **argv )
{
    assert( argc >= 2 );
    struct game_profile_catalog *catalog = calloc( 1, sizeof(*catalog) ); assert( catalog );
    for (int i = 2; i < argc; i++) assert( game_profiles_load( argv[i], catalog ) == GAME_PROFILE_INVALID );
    assert( game_profiles_load( argv[1], catalog ) == GAME_PROFILE_OK && catalog->count == 1 );
    struct game_profile profile = catalog->entries[0];
    assert( profile.file_count == 4 && profile.min_api == 13 );
    struct fixture f; fixture( &f ); int preserved;
    rename_count = 0;
    assert( game_profile_apply( f.settings, f.keys, &profile, "", "", &preserved ) == GAME_PROFILE_OK );
    int boundaries = rename_count;
    content( f.game, "new\0game", 8 ); content( f.user, "new user\r\n", 10 );
    content( f.nested, "new nested", 10 ); content( f.empty, "", 0 );
    assert( game_profile_restore( f.settings, f.keys ) == GAME_PROFILE_OK ); original( &f );
    /* Restore can be undone too, including the custom target mapping. */
    assert( game_profile_restore( f.settings, f.keys ) == GAME_PROFILE_OK );
    content( f.user, "new user\r\n", 10 );
    /* Updates use whole-file replacement and retain the player's previous bytes. */
    assert( durable_write( f.user, "player edit", 11 ) ); profile.version++;
    assert( game_profile_apply( f.settings, f.keys, &profile, "", "", &preserved ) == GAME_PROFILE_OK );
    content( f.user, "new user\r\n", 10 );
    assert( game_profile_restore( f.settings, f.keys ) == GAME_PROFILE_OK ); content( f.user, "player edit", 11 );

    for (int i = 1; i <= boundaries; i++)
    {
        struct fixture crash; fixture( &crash );
        pid_t pid = fork(); assert( pid >= 0 );
        if (!pid)
        {
            rename_count = 0; crash_after = i;
            game_profile_apply( crash.settings, crash.keys, &profile, "", "", &preserved ); _exit( 0 );
        }
        int status; assert( waitpid( pid, &status, 0 ) == pid && WIFEXITED( status ) && WEXITSTATUS( status ) == 73 );
        assert( game_profile_recover( crash.settings, crash.keys ) == GAME_PROFILE_OK ); original( &crash );
        struct fixture failed; fixture( &failed ); rename_count = 0; fail_after = i;
        assert( game_profile_apply( failed.settings, failed.keys, &profile, "", "", &preserved ) == GAME_PROFILE_IO );
        fail_after = 0; original( &failed );
    }
    /* An alias via another root, state target or a symlink must never overwrite it. */
    struct game_profile invalid = profile;
    strcpy( invalid.files[1].root, "drive_c" ); strcpy( invalid.files[1].path, "Game/config.ini" );
    assert( game_profile_apply( f.settings, f.keys, &invalid, "", "", &preserved ) == GAME_PROFILE_INVALID );
    invalid = profile; strcpy( invalid.files[0].path, "Game.wine-nx.txt.profile-backup" );
    assert( game_profile_apply( f.settings, f.keys, &invalid, "", "", &preserved ) == GAME_PROFILE_INVALID );
    struct fixture link; fixture( &link );
    assert( !unlink( link.game ) && !symlink( link.user, link.game ) );
    assert( game_profile_apply( link.settings, link.keys, &profile, "", "", &preserved ) == GAME_PROFILE_IO );
    content( link.user, "old user\r\n", 10 );
    struct fixture parent; fixture( &parent );
    char directory[768]; strcpy( directory, parent.user ); *strstr( directory, "/CAPCOM/" ) = 0;
    invalid = profile; strcpy( invalid.files[0].path, "alias/config.ini" );
    char alias[768]; strcpy( alias, parent.game ); strcpy( strrchr( alias, '/' ) + 1, "alias" );
    assert( !symlink( directory, alias ) );
    assert( game_profile_apply( parent.settings, parent.keys, &invalid, "", "", &preserved ) == GAME_PROFILE_IO );
    original( &parent );
    /* Backups written by API 12 retain their original 11-target CRC/layout. */
    struct pal3_header { uint32_t magic, crc, count, exists[11], sizes[11]; } old = {0};
    old.magic = PAL3_SNAPSHOT_MAGIC; old.count = 11; old.exists[0] = 1; old.sizes[0] = 8;
    old.crc = crc32( 0, (const void *)&old.count, sizeof(old) - offsetof(struct pal3_header, count) );
    old.crc = crc32( old.crc, (const void *)"title=ok", 8 );
    char legacy[800]; snprintf( legacy, sizeof(legacy), "%s.old-backup", parent.settings );
    FILE *file = fopen( legacy, "wb" ); assert( file );
    assert( fwrite( &old, 1, sizeof(old), file ) == sizeof(old) );
    assert( fwrite( "title=ok", 1, 8, file ) == 8 && !fclose( file ) );
    struct snapshot *converted = malloc( sizeof(*converted) ); assert( converted );
    assert( snapshot_read( legacy, converted ) && converted->count == 11 && converted->sizes[0] == 8 );
    assert( !memcmp( converted->data[0], "title=ok", 8 ) ); free( converted );
    game_profiles_clear( catalog ); free( catalog );
    printf( "profile files: custom roots, binary/empty files, nested creation, updates, restore, all %d crash/failure boundaries and aliases/symlinks passed\n", boundaries );
    return 0;
}
