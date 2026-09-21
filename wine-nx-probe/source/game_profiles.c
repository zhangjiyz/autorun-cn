/* A release contains bounded declarative resources, never executable payloads.
 * Apply settings with a durable rollback record; binary patches are hash-pinned
 * byte replacements applied by native code to the exact executable selected by the launcher. */
#include "game_profiles.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>
#include <minizip/unzip.h>
#include <zlib.h>
#include <png.h>
#ifdef __SWITCH__
#include <switch.h>
#else
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/sha.h>
#define SHA256_HASH_SIZE SHA256_DIGEST_LENGTH
typedef SHA256_CTX Sha256Context;
static void sha256ContextCreate( Sha256Context *c ) { SHA256_Init( c ); }
static void sha256ContextUpdate( Sha256Context *c, const void *p, size_t n ) { SHA256_Update( c, p, n ); }
static void sha256ContextGetHash( Sha256Context *c, void *out ) { SHA256_Final( out, c ); }
#endif

#define CATALOG_LIMIT (128 * 1024)
#define TARGET_COUNT 8
#define TEXT_TARGET_COUNT 7
#define LEGACY_SNAPSHOT_MAGIC 0x31504647u
#define SNAPSHOT_MAGIC 0x32504647u

static int identifier( const char *s )
{
    size_t n = strlen( s );
    if (!n || n >= 64) return 0;
    for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '-')) return 0;
    return 1;
}

static int positive_number( const char *text, unsigned int *out )
{
    char *end;
    unsigned long value;
    if (!*text || strspn( text, "0123456789" ) != strlen( text )) return 0;
    errno = 0;
    value = strtoul( text, &end, 10 );
    if (errno || *end || !value || value > INT_MAX) return 0;
    *out = value;
    return 1;
}

static int contains( const char *text, const char *query )
{
    size_t n = strlen( query );
    for (; *text; text++) if (!strncasecmp( text, query, n )) return 1;
    return !n;
}

int game_profile_matches( const struct game_profile *profile, const char *query )
{
    return contains( profile->name, query ) || contains( profile->keywords, query ) ||
           contains( profile->id, query );
}

/* Catalog wire format: version header, then six tab-separated UTF-8 columns.
 * The maintainer edits JSON; the packager generates this strict, small format. */
static int parse_catalog( char *text, struct game_profile_catalog *catalog )
{
    char *line, *next, *field[9];
    int v3 = !strncmp( text, "autorun-profiles-v3\n", 20 );
    int v2 = v3 || !strncmp( text, "autorun-profiles-v2\n", 20 ), fields = v3 ? 9 : v2 ? 8 : 6;
    if (!v2 && strncmp( text, "autorun-profiles-v1\n", 20 )) return 0;
    line = text + 20;
    catalog->count = 0;
    while (*line)
    {
        struct game_profile *p;
        if (catalog->count == GAME_PROFILE_MAX || !(next = strchr( line, '\n' ))) return 0;
        *next++ = 0;
        field[0] = line;
        for (int i = 1; i < fields; i++)
        {
            char *tab = strchr( field[i - 1], '\t' );
            if (!tab) return 0;
            *tab = 0; field[i] = tab + 1;
        }
        if (strchr( field[fields - 1], '\t' )) return 0;
        for (int i = 0; i < fields; i++) for (const unsigned char *c = (const void *)field[i]; *c; c++)
            if (*c < 32) return 0;
        p = &catalog->entries[catalog->count];
        if (!identifier( field[0] ) || !field[1][0] || strlen( field[1] ) >= sizeof(p->name) ||
            !positive_number( field[2], &p->version ) || !positive_number( field[3], &p->min_api ) ||
            strlen( field[4] ) >= sizeof(p->keywords) || strlen( field[5] ) >= sizeof(p->description)) return 0;
        if (v2)
        {
            if ((strcmp( field[6], "0" ) && strcmp( field[6], "1" )) ||
                (strcmp( field[7], "0" ) && strcmp( field[7], "1" )) || p->min_api < 2) return 0;
            p->has_cheats = field[6][0] == '1'; p->has_cover = field[7][0] == '1';
            if (v3)
            {
                if (strcmp( field[8], "0" ) && strcmp( field[8], "1" )) return 0;
                p->has_patch = field[8][0] == '1';
                if (p->has_patch && p->min_api < 3) return 0;
            }
        }
        for (int i = 0; i < catalog->count; i++) if (!strcmp( field[0], catalog->entries[i].id )) return 0;
        strcpy( p->id, field[0] ); strcpy( p->name, field[1] );
        strcpy( p->keywords, field[4] ); strcpy( p->description, field[5] );
        catalog->count++;
        line = next;
    }
    return catalog->count > 0;
}

/* One public index points to independently downloadable, hash-pinned game ZIPs. */
int game_profile_archive_name( const struct game_profile *profile, char *out, size_t size )
{
    int n = snprintf( out, size, "profile-%s-v%u.zip", profile->id, profile->version );
    return n >= 0 && (size_t)n < size;
}

int game_profile_index_parse( const char *text, struct game_profile_catalog *catalog )
{
    static const char header[] = "autorun-profile-index-v1\n";
    game_profiles_clear( catalog );
    if (strncmp( text, header, sizeof(header) - 1 )) return 0;
    text += sizeof(header) - 1;
    while (*text)
    {
        char line[2048], *field[9];
        size_t length = strcspn( text, "\n" );
        if (catalog->count == GAME_PROFILE_MAX || !text[length] || length >= sizeof(line)) goto invalid;
        memcpy( line, text, length ); line[length] = 0; text += length + 1;
        field[0] = line;
        for (int i = 1; i < 9; i++)
        {
            char *tab = strchr( field[i - 1], '\t' );
            if (!tab) goto invalid;
            *tab = 0; field[i] = tab + 1;
        }
        for (int i = 0; i < 9; i++) for (const unsigned char *c = (const void *)field[i]; *c; c++) if (*c < 32) goto invalid;
        struct game_profile *p = &catalog->entries[catalog->count];
        if (!identifier( field[0] ) || !field[1][0] || strlen( field[1] ) >= sizeof(p->name) ||
            !positive_number( field[2], &p->version ) || !positive_number( field[3], &p->min_api ) ||
            strlen( field[4] ) >= sizeof(p->keywords) || strlen( field[5] ) >= sizeof(p->description) ||
            strncmp( field[6], "https://", 8 ) || !field[6][8] || field[6][8] == '/' || strlen( field[6] ) >= sizeof(p->url) ||
            strpbrk( field[6] + 8, " \\#@" ) || strlen( field[7] ) != 64 ||
            strspn( field[7], "0123456789abcdef" ) != 64 || !positive_number( field[8], &p->archive_size ) ||
            p->archive_size > 16 * 1024 * 1024) goto invalid;
        for (int i = 0; i < catalog->count; i++) if (!strcmp( catalog->entries[i].id, field[0] )) goto invalid;
        strcpy( p->id, field[0] ); strcpy( p->name, field[1] ); strcpy( p->keywords, field[4] );
        strcpy( p->description, field[5] ); strcpy( p->url, field[6] ); strcpy( p->digest, field[7] );
        catalog->count++;
    }
    return catalog->count > 0;
 invalid:
    game_profiles_clear( catalog ); return 0;
}

static int read_zip_text( unzFile zip, char *out, size_t capacity )
{
    unz_file_info64 info;
    size_t used = 0;
    int n, ok;
    if (unzGetCurrentFileInfo64( zip, &info, NULL, 0, NULL, 0, NULL, 0 ) != UNZ_OK ||
        info.uncompressed_size >= capacity || (info.flag & 1) ||
        (((info.external_fa >> 16) & 0170000) && ((info.external_fa >> 16) & 0170000) != 0100000) ||
        (info.external_fa & 0x10) || unzOpenCurrentFile( zip ) != UNZ_OK) return 0;
    while (used < capacity - 1 && (n = unzReadCurrentFile( zip, out + used, capacity - 1 - used )) > 0) used += n;
    /* An extra read also detects streams whose declared length was dishonest. */
    char extra;
    ok = used == info.uncompressed_size && unzReadCurrentFile( zip, &extra, 1 ) == 0;
    ok = unzCloseCurrentFile( zip ) == UNZ_OK && ok;
    if (!ok || memchr( out, 0, used )) return 0;
    for (size_t i = 0; i < used; i++)
        if ((unsigned char)out[i] < 32 && out[i] != '\n' && out[i] != '\t' && out[i] != '\r') return 0;
    out[used] = 0;
    return 1;
}

static int hex_bytes( const char *text, unsigned char *out, unsigned int *size )
{
    size_t length = strlen( text );
    if (!length || length > GAME_PROFILE_PATCH_MAX * 2 || (length & 1) || strspn( text, "0123456789abcdef" ) != length) return 0;
    for (size_t i = 0; i < length / 2; i++)
    {
        unsigned int value;
        if (sscanf( text + i * 2, "%2x", &value ) != 1) return 0;
        out[i] = value;
    }
    *size = length / 2; return 1;
}

static int parse_binary_patch( const char *text, struct game_profile_patch *patch )
{
    static const char header[] = "autorun-binary-patch-v1\n";
    static const char *keys[] = {"original-sha256", "patched-sha256", "offset", "old", "new"};
    char values[5][160], *end;
    unsigned long long offset;
    unsigned int old_size, new_size;
    if (strncmp( text, header, sizeof(header) - 1 )) return 0;
    text += sizeof(header) - 1;
    for (int i = 0; i < 5; i++)
    {
        size_t key = strlen( keys[i] ), length = strcspn( text, "\n" );
        if (length <= key || text[key] != '=' || strncmp( text, keys[i], key ) || !text[length] ||
            length - key - 1 >= sizeof(values[i])) return 0;
        memcpy( values[i], text + key + 1, length - key - 1 ); values[i][length - key - 1] = 0;
        text += length + 1;
    }
    if (*text || strlen( values[0] ) != 64 || strlen( values[1] ) != 64 ||
        strspn( values[0], "0123456789abcdef" ) != 64 || strspn( values[1], "0123456789abcdef" ) != 64 ||
        !strcmp( values[0], values[1] )) return 0;
    errno = 0; offset = strtoull( values[2], &end, 10 );
    if (errno || !values[2][0] || *end || values[2][0] == '-' ||
        !hex_bytes( values[3], patch->old_bytes, &old_size ) ||
        !hex_bytes( values[4], patch->new_bytes, &new_size ) || old_size != new_size ||
        !memcmp( patch->old_bytes, patch->new_bytes, old_size )) return 0;
    memset( patch->original_digest, 0, sizeof(patch->original_digest) );
    memcpy( patch->original_digest, values[0], 64 ); memcpy( patch->patched_digest, values[1], 65 );
    patch->offset = offset; patch->size = old_size; return 1;
}

static int allowed_key( const char *key, int controls )
{
    static const char settings[] =
        "|title|d3d|d3d9|own-controls|controller|verbose|profile|window-fit|sdl-audio|sd-stat-cache|sd-clean-writer-cache|locale|wined3d-renderer|wined3d-frontbuffer-swap|wined3d-explicit-buffer-flush|wined3d-csmt|"
        "aspect-fit|touch-coordinates|left-stick-run|left-stick-eight-way|left-stick-aim|left-stick-move|"
        "windows|dxvk-version|vkd3d-version|dxvk-hud|frame-limit|vsync|";
    static const char keys[] =
        "|LSTICK|RSTICK|DPAD|TOUCH|UP|DOWN|LEFT|RIGHT|LUP|LDOWN|LLEFT|LRIGHT|RUP|RDOWN|RLEFT|RRIGHT|"
        "A|B|X|Y|L|R|ZL|ZR|PLUS|MINUS|STICKL|STICKR|";
    char needle[80];
    if (snprintf( needle, sizeof(needle), "|%s|", key ) >= (int)sizeof(needle)) return 0;
    return contains( controls ? keys : settings, needle );
}

/* Canonicalize defaults while rejecting duplicate/unknown keys. Local files retain
 * their comments and unknown settings; downloaded defaults never control paths. */
static int canonical_kv( const struct launcher_kv *in, struct launcher_kv *out, int controls )
{
    size_t pos = 0;
    memset( out, 0, sizeof(*out) );
    while (pos < in->size)
    {
        char line[512], key[64], value[384], previous[384], *eq, *start, *end;
        size_t length = strcspn( in->text + pos, "\n" );
        if (length >= sizeof(line)) return 0;
        memcpy( line, in->text + pos, length ); line[length] = 0;
        pos += length + (pos + length < in->size);
        start = line; while (*start == ' ' || *start == '\t') start++;
        if (!*start || *start == '#' || *start == ';' || *start == '\r') continue;
        eq = strchr( start, '=' ); if (!eq) return 0;
        end = eq; while (end > start && isspace( (unsigned char)end[-1] )) end--;
        if (end == start || (size_t)(end - start) >= sizeof(key)) return 0;
        memcpy( key, start, end - start ); key[end - start] = 0;
        if (!allowed_key( key, controls ) || launcher_kv_get( out, key, previous, sizeof(previous) )) return 0;
        start = eq + 1; while (*start && isspace( (unsigned char)*start )) start++;
        end = start + strlen( start ); while (end > start && isspace( (unsigned char)end[-1] )) end--;
        if (end == start || (size_t)(end - start) >= sizeof(value)) return 0;
        memcpy( value, start, end - start ); value[end - start] = 0;
        if (!launcher_kv_set( out, key, value )) return 0;
    }
    return 1;
}

static uint32_t png_u32( const unsigned char *data )
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
}

/* libpng's simplified decoder may finish once pixels are decoded, even if the
 * end chunk is missing. Require a complete, CRC-valid PNG container as well. */
static int complete_png( const unsigned char *data, size_t size )
{
    size_t pos = 8;
    int header = 0, pixels = 0;
    if (size < 8 || png_sig_cmp( data, 0, 8 )) return 0;
    while (pos < size)
    {
        if (size - pos < 12) return 0;
        uint32_t length = png_u32( data + pos );
        const unsigned char *kind = data + pos + 4;
        if (length > size - pos - 12 || crc32( 0, kind, length + 4 ) != png_u32( kind + 4 + length )) return 0;
        if (!header) { if (memcmp( kind, "IHDR", 4 ) || length != 13) return 0; header = 1; }
        else if (!memcmp( kind, "IHDR", 4 )) return 0;
        if (!memcmp( kind, "IDAT", 4 )) pixels = 1;
        pos += length + 12;
        if (!memcmp( kind, "IEND", 4 )) return !length && pixels && pos == size;
    }
    return 0;
}

static int read_zip_cover( unzFile zip, struct game_profile *profile )
{
    unz_file_info64 info;
    png_image png = {0};
    unsigned char *pixels = NULL;
    int ok = 0, n, opened = 0;
    unsigned int used = 0;
    if (unzGetCurrentFileInfo64( zip, &info, NULL, 0, NULL, 0, NULL, 0 ) != UNZ_OK ||
        !info.uncompressed_size || info.uncompressed_size > GAME_PROFILE_COVER_MAX || (info.flag & 1) ||
        (((info.external_fa >> 16) & 0170000) && ((info.external_fa >> 16) & 0170000) != 0100000) ||
        (info.external_fa & 0x10) || !(profile->cover = malloc( info.uncompressed_size ))) return 0;
    profile->cover_size = info.uncompressed_size;
    if (unzOpenCurrentFile( zip ) != UNZ_OK) goto done;
    opened = 1;
    while (used < profile->cover_size && (n = unzReadCurrentFile( zip, profile->cover + used, profile->cover_size - used )) > 0) used += n;
    unsigned char extra;
    if (used != profile->cover_size || unzReadCurrentFile( zip, &extra, 1 ) != 0) goto done;
    opened = 0;
    if (unzCloseCurrentFile( zip ) != UNZ_OK) goto done;
    if (!complete_png( profile->cover, profile->cover_size )) goto done;
    png.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory( &png, profile->cover, profile->cover_size )) goto done;
    if (!png.width || !png.height || png.width > 2048 || png.height > 2048) goto done;
    png.format = PNG_FORMAT_RGBA;
    pixels = malloc( PNG_IMAGE_SIZE( png ) );
    ok = pixels && png_image_finish_read( &png, NULL, pixels, 0, NULL );
 done:
    if (opened) unzCloseCurrentFile( zip );
    png_image_free( &png ); free( pixels );
    if (!ok) { free( profile->cover ); profile->cover = NULL; profile->cover_size = 0; }
    return ok;
}

void game_profiles_clear( struct game_profile_catalog *catalog )
{
    for (int i = 0; i < GAME_PROFILE_MAX; i++) free( catalog->entries[i].cover );
    memset( catalog, 0, sizeof(*catalog) );
}

enum game_profile_result game_profiles_load( const char *archive, struct game_profile_catalog *catalog )
{
    unzFile zip = unzOpen64( archive );
    unz_global_info64 global;
    unsigned char seen[GAME_PROFILE_MAX][5] = {{0}};
    char *text = malloc( CATALOG_LIMIT );
    struct launcher_kv *raw = malloc( sizeof(*raw) );
    struct game_cheats *cheats = malloc( sizeof(*cheats) );
    enum game_profile_result result = GAME_PROFILE_INVALID;
    int index_count = 0, next, expected_count = 1;
    unsigned long long cover_total = 0;
    game_profiles_clear( catalog );
    if (!zip || !text || !raw || !cheats) { result = GAME_PROFILE_IO; goto done; }
    if (unzGetGlobalInfo64( zip, &global ) != UNZ_OK || !global.number_entry ||
        global.number_entry > 1 + GAME_PROFILE_MAX * 5 || unzLocateFile( zip, "catalog.tsv", 1 ) != UNZ_OK ||
        !read_zip_text( zip, text, CATALOG_LIMIT ) || !parse_catalog( text, catalog )) goto done;
    for (int i = 0; i < catalog->count; i++) expected_count += 2 + catalog->entries[i].has_cheats +
        catalog->entries[i].has_cover + catalog->entries[i].has_patch;
    if (global.number_entry != (unsigned)expected_count || unzGoToFirstFile( zip ) != UNZ_OK) goto done;
    do
    {
        char name[128], expected[128];
        unz_file_info64 info;
        int found = 0;
        if (unzGetCurrentFileInfo64( zip, &info, name, sizeof(name), NULL, 0, NULL, 0 ) != UNZ_OK ||
            info.size_filename >= sizeof(name) || strlen( name ) != info.size_filename) goto done;
        if (!strcmp( name, "catalog.tsv" )) { if (++index_count != 1) goto done; continue; }
        for (int i = 0; i < catalog->count && !found; i++) for (int k = 0; k < 5; k++)
        {
            struct game_profile *p = &catalog->entries[i];
            static const char *files[] = {"settings.txt", "keys.txt", "cheats.txt", "cover.png", "patch.txt"};
            if ((k == 2 && !p->has_cheats) || (k == 3 && !p->has_cover) || (k == 4 && !p->has_patch)) continue;
            snprintf( expected, sizeof(expected), "%s/%s", p->id, files[k] );
            if (strcmp( name, expected )) continue;
            if (seen[i][k]++) goto done;
            if (k == 3)
            {
                cover_total += info.uncompressed_size;
                if (cover_total > 16 * 1024 * 1024 || !read_zip_cover( zip, p )) goto done;
            }
            else
            {
                if (!read_zip_text( zip, raw->text, sizeof(raw->text) )) goto done;
                raw->size = strlen( raw->text );
                if (k == 2)
                {
                    if (!game_cheats_parse( raw->text, cheats )) goto done;
                    p->cheats = *raw;
                }
                else if (k == 4)
                {
                    if (!parse_binary_patch( raw->text, &p->patch )) goto done;
                }
                else if (!canonical_kv( raw, k ? &p->keys : &p->settings, k )) goto done;
            }
            found = 1; break;
        }
        if (!found) goto done;
    } while ((next = unzGoToNextFile( zip )) == UNZ_OK);
    if (next != UNZ_END_OF_LIST_OF_FILE || index_count != 1) goto done;
    for (int i = 0; i < catalog->count; i++)
        if (!seen[i][0] || !seen[i][1] || seen[i][2] != catalog->entries[i].has_cheats ||
            seen[i][3] != catalog->entries[i].has_cover || seen[i][4] != catalog->entries[i].has_patch) goto done;
    result = GAME_PROFILE_OK;
 done:
    if (zip) unzClose( zip );
    free( cheats ); free( raw ); free( text );
    if (result != GAME_PROFILE_OK) game_profiles_clear( catalog );
    return result;
}

struct snapshot
{
    uint32_t magic, crc, count, exists[TARGET_COUNT], sizes[TARGET_COUNT];
    char data[TEXT_TARGET_COUNT][LAUNCHER_KV_MAX];
    char cover[GAME_PROFILE_COVER_MAX];
};

#define SNAP_DATA(s, i) ((i) == 7 ? (s)->cover : (s)->data[i])

struct targets { char paths[TARGET_COUNT][768], pending[800], backup[800]; };

static int target_paths( struct targets *t, const char *settings, const char *keys )
{
    const char *suffixes[] = {"", "", ".adaptation", ".profile-settings", ".profile-keys", ".cheats", ".cheat-options", ".profile-cover.png"};
    for (int i = 0; i < TARGET_COUNT; i++)
        if (snprintf( t->paths[i], sizeof(t->paths[i]), "%s%s", i == 1 ? keys : settings, suffixes[i] ) >= (int)sizeof(t->paths[i])) return 0;
    snprintf( t->pending, sizeof(t->pending), "%s.profile-pending", settings );
    snprintf( t->backup, sizeof(t->backup), "%s.profile-backup", settings );
    return 1;
}

/* Do not follow a symlink in a game folder, state file or temporary file. */
static int safe_path( const char *path )
{
    char copy[1024];
    struct stat st;
    size_t len = strlen( path );
    if (!len || len >= sizeof(copy)) return 0;
    strcpy( copy, path );
    for (size_t i = 0; i <= len; i++) if ((copy[i] == '/' && i) || i == len)
    {
        char c = copy[i]; copy[i] = 0;
        /* devoptab mount prefix (sdmc:) is not a directory by itself. */
        if (copy[i - 1] != ':')
        {
            if (lstat( copy, &st )) { if (errno != ENOENT || i != len) return 0; }
            else if (S_ISLNK( st.st_mode ) || (i != len ? !S_ISDIR( st.st_mode ) : !S_ISREG( st.st_mode ))) return 0;
        }
        copy[i] = c;
    }
    return 1;
}

static int read_local( const char *path, char *out, uint32_t *size, uint32_t *exists )
{
    FILE *file;
    size_t n;
    if (!safe_path( path )) return 0;
    errno = 0;
    file = fopen( path, "rb" );
    if (!file) { *size = *exists = 0; return errno == ENOENT; }
    n = fread( out, 1, LAUNCHER_KV_MAX, file );
    int ok = n < LAUNCHER_KV_MAX && !ferror( file ) && !memchr( out, 0, n );
    fclose( file );
    if (!ok) return 0;
    *size = n; *exists = 1; out[n] = 0;
    return 1;
}

static int sync_parent( const char *path )
{
#ifdef __SWITCH__
    char device[32];
    const char *colon = strchr( path, ':' );
    if (!colon || colon - path >= (int)sizeof(device)) return 0;
    memcpy( device, path, colon - path ); device[colon - path] = 0;
    return R_SUCCEEDED( fsdevCommitDevice( device ) );
#else
    char parent[1024], *slash;
    int fd, ok;
    if (strlen( path ) >= sizeof(parent)) return 0;
    strcpy( parent, path );
    slash = strrchr( parent, '/' );
    if (slash) { if (slash == parent) slash[1] = 0; else *slash = 0; }
    else strcpy( parent, "." );
    if ((fd = open( parent, O_RDONLY | O_DIRECTORY )) < 0) return 0;
    ok = !fsync( fd ); close( fd ); return ok;
#endif
}

static int erase_durable( const char *path )
{
    return (!remove( path ) || errno == ENOENT) && sync_parent( path );
}

static int durable_write( const char *path, const void *data, size_t size )
{
    char temp[1024];
    FILE *file;
    if (snprintf( temp, sizeof(temp), "%s.new", path ) >= (int)sizeof(temp) || !safe_path( path ) || !safe_path( temp )) return 0;
    if (!(file = fopen( temp, "wb" ))) return 0;
    int ok = fwrite( data, 1, size, file ) == size;
    if (fflush( file ) || fsync( fileno( file ) )) ok = 0;
    if (fclose( file )) ok = 0;
    if (!ok) { remove( temp ); return 0; }
    if (remove( path ) && errno != ENOENT) return 0;
    return !rename( temp, path ) && sync_parent( path );
}

static int patch_paths( const char *exe, char *backup, char *state, char *temp )
{
    return strlen( exe ) < 700 &&
        snprintf( backup, 768, "%s.autorun-before-profile-patch", exe ) < 768 &&
        snprintf( state, 768, "%s.autorun-profile-patch", exe ) < 768 &&
        snprintf( temp, 768, "%s.autorun-patch.tmp", exe ) < 768;
}

static int file_digest( const char *path, char digest[65] )
{
    unsigned char data[65536], hash[SHA256_HASH_SIZE];
    static const char hex[] = "0123456789abcdef";
    Sha256Context context;
    FILE *file;
    size_t n;
    if (!safe_path( path ) || !(file = fopen( path, "rb" ))) return 0;
    sha256ContextCreate( &context );
    while ((n = fread( data, 1, sizeof(data), file ))) sha256ContextUpdate( &context, data, n );
    int ok = !ferror( file );
    if (fclose( file )) ok = 0;
    if (!ok) return 0;
    sha256ContextGetHash( &context, hash );
    for (size_t i = 0; i < sizeof(hash); i++) { digest[i * 2] = hex[hash[i] >> 4]; digest[i * 2 + 1] = hex[hash[i] & 15]; }
    digest[64] = 0; return 1;
}

static int copy_durable( const char *source, const char *destination )
{
    char temp[800];
    unsigned char data[65536];
    FILE *in = NULL, *out = NULL;
    size_t n;
    int ok = 0;
    if (snprintf( temp, sizeof(temp), "%s.new", destination ) >= (int)sizeof(temp) ||
        !safe_path( source ) || !safe_path( destination ) || !safe_path( temp ) ||
        !(in = fopen( source, "rb" )) || !(out = fopen( temp, "wb" ))) goto done;
    while ((n = fread( data, 1, sizeof(data), in ))) if (fwrite( data, 1, n, out ) != n) goto done;
    if (ferror( in ) || fflush( out ) || fsync( fileno( out ) )) goto done;
    if (fclose( out )) { out = NULL; goto done; } out = NULL;
    if ((remove( destination ) && errno != ENOENT) || rename( temp, destination ) || !sync_parent( destination )) goto done;
    ok = 1;
 done:
    if (in) fclose( in );
    if (out) fclose( out );
    if (!ok) remove( temp );
    return ok;
}

static int patch_text( const struct game_profile_patch *patch, char *text, size_t capacity )
{
    static const char hex[] = "0123456789abcdef";
    char old[GAME_PROFILE_PATCH_MAX * 2 + 1], next[GAME_PROFILE_PATCH_MAX * 2 + 1];
    if (!patch->size || patch->size > GAME_PROFILE_PATCH_MAX || strlen( patch->original_digest ) != 64 ||
        strlen( patch->patched_digest ) != 64 || strspn( patch->original_digest, "0123456789abcdef" ) != 64 ||
        strspn( patch->patched_digest, "0123456789abcdef" ) != 64 ||
        !strcmp( patch->original_digest, patch->patched_digest ) ||
        !memcmp( patch->old_bytes, patch->new_bytes, patch->size )) return 0;
    for (unsigned int i = 0; i < patch->size; i++)
    {
        old[i * 2] = hex[patch->old_bytes[i] >> 4]; old[i * 2 + 1] = hex[patch->old_bytes[i] & 15];
        next[i * 2] = hex[patch->new_bytes[i] >> 4]; next[i * 2 + 1] = hex[patch->new_bytes[i] & 15];
    }
    old[patch->size * 2] = next[patch->size * 2] = 0;
    int n = snprintf( text, capacity, "autorun-binary-patch-v1\noriginal-sha256=%s\npatched-sha256=%s\n"
        "offset=%llu\nold=%s\nnew=%s\n", patch->original_digest, patch->patched_digest,
        patch->offset, old, next );
    return n > 0 && (size_t)n < capacity;
}

static int patch_state_read( const char *state, struct game_profile_patch *patch, int *exists )
{
    char text[1024];
    FILE *file;
    size_t size;
    *exists = 0;
    if (!safe_path( state )) return 0;
    if (!(file = fopen( state, "rb" ))) return errno == ENOENT;
    size = fread( text, 1, sizeof(text) - 1, file );
    int ok = size < sizeof(text) - 1 && fgetc( file ) == EOF && !ferror( file );
    if (fclose( file )) ok = 0;
    if (!ok || memchr( text, 0, size )) return 0;
    text[size] = 0; *exists = 1; memset( patch, 0, sizeof(*patch));
    return parse_binary_patch( text, patch );
}

enum game_profile_result game_profile_patch_recover( const char *exe )
{
    char backup[768], state[768], temp[768], digest[65];
    struct game_profile_patch patch;
    struct stat st;
    int exists;
    if (!patch_paths( exe, backup, state, temp ) || !patch_state_read( state, &patch, &exists )) return GAME_PROFILE_RECOVERY;
    if (!exists) return GAME_PROFILE_OK;
    if (file_digest( exe, digest ))
    {
        if (!strcmp( digest, patch.patched_digest )) return GAME_PROFILE_OK;
        if (!strcmp( digest, patch.original_digest )) return erase_durable( state ) ? GAME_PROFILE_OK : GAME_PROFILE_RECOVERY;
        return GAME_PROFILE_RECOVERY;
    }
    if (!lstat( exe, &st ) || errno != ENOENT || !file_digest( backup, digest ) ||
        strcmp( digest, patch.original_digest ) || !copy_durable( backup, exe ) || !erase_durable( state ))
        return GAME_PROFILE_RECOVERY;
    remove( temp ); return GAME_PROFILE_OK;
}

enum game_profile_result game_profile_patch_apply( const char *exe, const struct game_profile_patch *patch, int *changed )
{
    char backup[768], state[768], temp[768], digest[65], text[1024];
    unsigned char current[GAME_PROFILE_PATCH_MAX];
    struct stat st;
    FILE *file;
    int state_exists;
    struct game_profile_patch active;
    *changed = 0;
    if (!patch_paths( exe, backup, state, temp ) || !patch_text( patch, text, sizeof(text)) ||
        patch->offset > ULLONG_MAX - patch->size) return GAME_PROFILE_INVALID;
    enum game_profile_result recovered = game_profile_patch_recover( exe );
    if (recovered != GAME_PROFILE_OK) return recovered;
    if (!file_digest( exe, digest )) return GAME_PROFILE_IO;
    if (!strcmp( digest, patch->patched_digest )) return GAME_PROFILE_OK;
    if (strcmp( digest, patch->original_digest )) return GAME_PROFILE_UNSUPPORTED;
    if (!(file = fopen( exe, "rb" ))) return GAME_PROFILE_IO;
    int bytes_ok = !fseeko( file, patch->offset, SEEK_SET ) &&
        fread( current, 1, patch->size, file ) == patch->size &&
        !ferror( file ) && !memcmp( current, patch->old_bytes, patch->size );
    if (fclose( file )) bytes_ok = 0;
    if (!bytes_ok) return GAME_PROFILE_UNSUPPORTED;
    if (!lstat( backup, &st ))
    {
        if (!S_ISREG( st.st_mode ) || !file_digest( backup, digest ) || strcmp( digest, patch->original_digest )) return GAME_PROFILE_RECOVERY;
    }
    else if (errno != ENOENT || !copy_durable( exe, backup ) || !file_digest( backup, digest ) ||
             strcmp( digest, patch->original_digest )) return GAME_PROFILE_IO;
    if (!patch_state_read( state, &active, &state_exists ) || state_exists ||
        !durable_write( state, text, strlen(text) ) || !copy_durable( exe, temp )) return GAME_PROFILE_IO;
    file = fopen( temp, "r+b" );
    int write_ok = file && !fseeko( file, patch->offset, SEEK_SET ) &&
        fwrite( patch->new_bytes, 1, patch->size, file ) == patch->size &&
        !fflush( file ) && !fsync( fileno( file ) );
    if (file && fclose( file )) write_ok = 0;
    if (!write_ok || !file_digest( temp, digest ) || strcmp( digest, patch->patched_digest ))
    { remove( temp ); erase_durable( state ); return GAME_PROFILE_IO; }
    if ((remove( exe ) && errno != ENOENT) || rename( temp, exe ) || !sync_parent( exe ))
    {
        remove( temp );
        if (!copy_durable( backup, exe ) || !erase_durable( state )) return GAME_PROFILE_RECOVERY;
        return GAME_PROFILE_IO;
    }
    *changed = 1; return GAME_PROFILE_OK;
}

enum game_profile_result game_profile_patch_restore( const char *exe )
{
    char backup[768], state[768], temp[768], digest[65];
    struct game_profile_patch patch;
    int exists;
    if (!patch_paths( exe, backup, state, temp ) || !patch_state_read( state, &patch, &exists )) return GAME_PROFILE_RECOVERY;
    if (!exists) return GAME_PROFILE_OK;
    enum game_profile_result recovered = game_profile_patch_recover( exe );
    if (recovered != GAME_PROFILE_OK || !file_digest( exe, digest )) return GAME_PROFILE_RECOVERY;
    if (!strcmp( digest, patch.original_digest )) return erase_durable( state ) ? GAME_PROFILE_OK : GAME_PROFILE_RECOVERY;
    if (strcmp( digest, patch.patched_digest ) || !file_digest( backup, digest ) ||
        strcmp( digest, patch.original_digest ) || !copy_durable( backup, exe ) ||
        !file_digest( exe, digest ) || strcmp( digest, patch.original_digest ) || !erase_durable( state ))
        return GAME_PROFILE_RECOVERY;
    return GAME_PROFILE_OK;
}

static uint32_t snapshot_crc( const struct snapshot *s )
{
    uint32_t crc = crc32( 0, (const void *)&s->count, offsetof(struct snapshot, data) - offsetof(struct snapshot, count) );
    for (unsigned int i = 0; i < s->count; i++) crc = crc32( crc, (const void *)SNAP_DATA(s, i), s->sizes[i] );
    return crc;
}

static int snapshot_read( const char *path, struct snapshot *s )
{
    FILE *file;
    uint32_t magic;
    int ok = 0;
    if (!safe_path( path ) || !(file = fopen( path, "rb" ))) return 0;
    memset( s, 0, sizeof(*s) );
    if (fread( &magic, 1, sizeof(magic), file ) != sizeof(magic)) goto done;
    rewind( file );
    if (magic == LEGACY_SNAPSHOT_MAGIC)
    {
        /* v21 wrote a fixed five-file record. It must remain recoverable after
         * upgrading the NRO, with the new optional files absent, as they were in v21. */
        struct legacy_snapshot { uint32_t magic, crc, exists[5], sizes[5]; char data[5][LAUNCHER_KV_MAX]; };
        struct legacy_snapshot *old = malloc( sizeof(*old) );
        if (!old) goto done;
        ok = fread( old, 1, sizeof(*old), file ) == sizeof(*old) && fgetc( file ) == EOF && !ferror( file );
        if (ok) ok = old->crc == crc32( 0, (const void *)old->exists, sizeof(*old) - offsetof(struct legacy_snapshot, exists) );
        for (int i = 0; ok && i < 5; i++)
        {
            if (old->exists[i] > 1 || old->sizes[i] >= LAUNCHER_KV_MAX || old->data[i][old->sizes[i]] ||
                memchr( old->data[i], 0, old->sizes[i] )) { ok = 0; break; }
            s->exists[i] = old->exists[i]; s->sizes[i] = old->sizes[i];
            memcpy( s->data[i], old->data[i], old->sizes[i] );
        }
        free( old ); s->magic = SNAPSHOT_MAGIC; s->count = TARGET_COUNT; s->crc = snapshot_crc( s );
        goto done;
    }
    if (magic != SNAPSHOT_MAGIC || fread( s, 1, offsetof(struct snapshot, data), file ) != offsetof(struct snapshot, data) ||
        (s->count != 5 && s->count != TARGET_COUNT)) goto done;
    for (unsigned int i = 0; i < s->count; i++)
    {
        if (s->exists[i] > 1 || (!s->exists[i] && s->sizes[i]) ||
            (i == 7 ? s->sizes[i] > GAME_PROFILE_COVER_MAX : s->sizes[i] >= LAUNCHER_KV_MAX)) goto done;
        if (fread( SNAP_DATA(s, i), 1, s->sizes[i], file ) != s->sizes[i] ||
            (i != 7 && memchr( s->data[i], 0, s->sizes[i] ))) goto done;
    }
    ok = fgetc( file ) == EOF && !ferror( file ) && s->crc == snapshot_crc( s );
 done:
    fclose( file ); return ok;
}

static int snapshot_write( const char *path, const struct snapshot *s )
{
    size_t size = offsetof(struct snapshot, data), pos = size;
    for (unsigned int i = 0; i < s->count; i++) size += s->sizes[i];
    unsigned char *data = malloc( size );
    if (!data) return 0;
    memcpy( data, s, pos );
    uint32_t crc = snapshot_crc( s ); memcpy( data + offsetof(struct snapshot, crc), &crc, sizeof(crc) );
    for (unsigned int i = 0; i < s->count; i++)
    { memcpy( data + pos, SNAP_DATA(s, i), s->sizes[i] ); pos += s->sizes[i]; }
    int ok = durable_write( path, data, size ); free( data ); return ok;
}

static int snapshot_capture( const struct targets *t, struct snapshot *s )
{
    memset( s, 0, sizeof(*s) ); s->magic = SNAPSHOT_MAGIC; s->count = TARGET_COUNT;
    for (int i = 0; i < TEXT_TARGET_COUNT; i++)
        if (!read_local( t->paths[i], s->data[i], &s->sizes[i], &s->exists[i] )) return 0;
    FILE *file;
    if (!safe_path( t->paths[7] )) return 0;
    if ((file = fopen( t->paths[7], "rb" )))
    {
        s->sizes[7] = fread( s->cover, 1, sizeof(s->cover), file ); s->exists[7] = 1;
        int ok = fgetc( file ) == EOF && !ferror( file ); fclose( file );
        if (!ok) return 0;
    }
    else if (errno != ENOENT) return 0;
    s->crc = snapshot_crc( s ); return 1;
}

static int snapshot_apply( const struct targets *t, const struct snapshot *s, const struct snapshot *previous )
{
    for (unsigned int i = 0; i < s->count; i++)
    {
        if (previous && previous->exists[i] == s->exists[i] && previous->sizes[i] == s->sizes[i] &&
            !memcmp( SNAP_DATA(previous, i), SNAP_DATA(s, i), s->sizes[i] )) continue;
        if (!safe_path( t->paths[i] )) return 0;
        if (s->exists[i]) { if (!durable_write( t->paths[i], SNAP_DATA(s, i), s->sizes[i] )) return 0; }
        else if (!erase_durable( t->paths[i] )) return 0;
    }
    return 1;
}

static int snapshot_restore( const struct targets *t, const struct snapshot *s )
{
    return snapshot_apply( t, s, NULL );
}

enum game_profile_result game_profile_recover( const char *settings, const char *keys )
{
    struct targets t;
    struct stat st;
    struct snapshot *s;
    if (!target_paths( &t, settings, keys )) return GAME_PROFILE_IO;
    if (lstat( t.pending, &st )) return errno == ENOENT ? GAME_PROFILE_OK : GAME_PROFILE_RECOVERY;
    s = malloc( sizeof(*s) );
    int ok = s && snapshot_read( t.pending, s ) && snapshot_restore( &t, s ) && erase_durable( t.pending );
    free( s );
    return ok ? GAME_PROFILE_OK : GAME_PROFILE_RECOVERY;
}

enum game_profile_result game_profile_binding_read( const char *settings, struct game_profile_binding *binding )
{
    struct targets t;
    struct launcher_kv kv;
    uint32_t size, exists;
    char version[32];
    memset( binding, 0, sizeof(*binding) );
    if (!target_paths( &t, settings, settings ) || !read_local( t.paths[2], kv.text, &size, &exists )) return GAME_PROFILE_IO;
    if (!exists) return GAME_PROFILE_OK;
    kv.size = size;
    if (!launcher_kv_get( &kv, "id", binding->id, sizeof(binding->id) ) || !identifier( binding->id ) ||
        !launcher_kv_get( &kv, "name", binding->name, sizeof(binding->name) ) ||
        !launcher_kv_get( &kv, "repository", binding->repository, sizeof(binding->repository) ) ||
        !launcher_kv_get( &kv, "tag", binding->tag, sizeof(binding->tag) ) ||
        !launcher_kv_get( &kv, "version", version, sizeof(version) ) || !positive_number( version, &binding->version )) return GAME_PROFILE_INVALID;
    return GAME_PROFILE_OK;
}

/* Missing keys deliberately deleted by the player are also local edits. */
static int merge_defaults( struct launcher_kv *current, const struct launcher_kv *old,
                           const struct launcher_kv *next, int first, int *preserved )
{
    struct launcher_kv union_keys = *old;
    size_t pos = 0;
    for (int pass = 0; pass < 2; pass++)
    {
        const struct launcher_kv *source = pass ? &union_keys : next;
        pos = 0;
        while (pos < source->size)
        {
            char key[64], value[384], was[384], now[LAUNCHER_KV_MAX];
            const char *eq = strchr( source->text + pos, '=' );
            size_t length = strcspn( source->text + pos, "\n" );
            if (!eq || (size_t)(eq - source->text - pos) >= sizeof(key) || eq >= source->text + pos + length) return 0;
            memcpy( key, source->text + pos, eq - source->text - pos ); key[eq - source->text - pos] = 0;
            const char *new_value = launcher_kv_get( next, key, value, sizeof(value) );
            const char *old_value = launcher_kv_get( old, key, was, sizeof(was) );
            const char *local = launcher_kv_get( current, key, now, sizeof(now) );
            if (!pass)
            {
                if (!launcher_kv_set( &union_keys, key, new_value )) return 0;
            }
            else if ((first && new_value) || (local && old_value && !strcmp( local, old_value )) || (!local && !old_value))
            {
                if (!launcher_kv_set( current, key, new_value )) return 0;
            }
            else if ((new_value || local) && (!new_value || !local || strcmp( new_value, local ))) (*preserved)++;
            pos += length + (pos + length < source->size);
        }
    }
    return 1;
}

static enum game_profile_result commit( const struct targets *t, const struct snapshot *before,
                                        const struct snapshot *after, int backup )
{
    if (!snapshot_write( t->pending, before )) return GAME_PROFILE_IO;
    if (snapshot_apply( t, after, before ) && (!backup || snapshot_write( t->backup, before )) && erase_durable( t->pending )) return GAME_PROFILE_OK;
    if (snapshot_restore( t, before ) && erase_durable( t->pending )) return GAME_PROFILE_IO;
    return GAME_PROFILE_RECOVERY;
}

enum game_profile_result game_profile_apply( const char *settings, const char *keys,
        const struct game_profile *profile, const char *repository, const char *tag, int *preserved )
{
    struct targets t;
    struct game_profile_binding binding;
    struct snapshot *before = NULL, *after = NULL;
    struct launcher_kv *kv = NULL, *old = NULL;
    enum game_profile_result result = game_profile_recover( settings, keys );
    int first;
    *preserved = 0;
    if (result != GAME_PROFILE_OK) return result;
    if (profile->cover_size > GAME_PROFILE_COVER_MAX || (profile->cover_size && !profile->cover) ||
        profile->cheats.size >= LAUNCHER_KV_MAX) return GAME_PROFILE_INVALID;
    if (profile->min_api > GAME_PROFILE_API) return GAME_PROFILE_INCOMPATIBLE;
    if ((result = game_profile_binding_read( settings, &binding )) != GAME_PROFILE_OK) return result;
    first = strcmp( binding.id, profile->id ) || strcmp( binding.repository, repository ) || strcmp( binding.tag, tag );
    if (!first && profile->version < binding.version) return GAME_PROFILE_OLD;
    result = GAME_PROFILE_IO;
    if (!target_paths( &t, settings, keys ) || !(before = malloc( sizeof(*before) )) ||
        !(after = malloc( sizeof(*after) )) || !(kv = malloc( sizeof(*kv) )) || !(old = malloc( sizeof(*old) )) ) goto done;
    if (!snapshot_capture( &t, before )) goto done;
    *after = *before;
    for (int i = 0; i < 2; i++)
    {
        const struct launcher_kv *next = i ? &profile->keys : &profile->settings;
        memcpy( kv->text, before->data[i], LAUNCHER_KV_MAX ); kv->size = before->sizes[i];
        memcpy( old->text, before->data[3 + i], LAUNCHER_KV_MAX ); old->size = before->sizes[3 + i];
        /* Baselines were canonicalized on download. Validate them again after disk I/O. */
        struct launcher_kv canonical;
        if (!canonical_kv( old, &canonical, i ) || !merge_defaults( kv, &canonical, next, first, preserved )) goto done;
        memcpy( after->data[i], kv->text, LAUNCHER_KV_MAX ); after->sizes[i] = kv->size; after->exists[i] = 1;
        memcpy( after->data[3 + i], next->text, LAUNCHER_KV_MAX ); after->sizes[3 + i] = next->size; after->exists[3 + i] = 1;
    }
    memset( kv, 0, sizeof(*kv) );
    char version[32]; snprintf( version, sizeof(version), "%u", profile->version );
    if (!launcher_kv_set( kv, "id", profile->id ) || !launcher_kv_set( kv, "name", profile->name ) ||
        !launcher_kv_set( kv, "version", version ) || !launcher_kv_set( kv, "repository", repository ) ||
        !launcher_kv_set( kv, "tag", tag )) goto done;
    memcpy( after->data[2], kv->text, LAUNCHER_KV_MAX ); after->sizes[2] = kv->size; after->exists[2] = 1;
    /* Definitions, private choices and the automatic cover participate in the
     * same transaction as the binding. Switching profiles resets cheat choices. */
    struct game_cheats old_cheats = {0}, next_cheats = {0};
    struct launcher_kv options = {0}, reconciled;
    if ((before->sizes[5] && !game_cheats_parse( before->data[5], &old_cheats )) ||
        (profile->cheats.size && !game_cheats_parse( profile->cheats.text, &next_cheats )))
    { result = GAME_PROFILE_INVALID; goto done; }
    options.size = before->sizes[6]; memcpy( options.text, before->data[6], LAUNCHER_KV_MAX );
    if (!game_cheats_reconcile( &old_cheats, &next_cheats, &options, first, &reconciled )) goto done;
    after->sizes[5] = profile->cheats.size; after->exists[5] = !!profile->cheats.size;
    memcpy( after->data[5], profile->cheats.text, LAUNCHER_KV_MAX );
    after->sizes[6] = reconciled.size; after->exists[6] = 1; memcpy( after->data[6], reconciled.text, LAUNCHER_KV_MAX );
    after->sizes[7] = profile->cover_size; after->exists[7] = !!profile->cover_size;
    if (profile->cover_size) memcpy( after->cover, profile->cover, profile->cover_size );
    kv->size = after->sizes[0]; memcpy( kv->text, after->data[0], LAUNCHER_KV_MAX );
    if (!launcher_kv_set( kv, "profile-cover", profile->cover_size ? "1" : NULL )) goto done;
    after->sizes[0] = kv->size; memcpy( after->data[0], kv->text, LAUNCHER_KV_MAX );
    result = commit( &t, before, after, 1 );
 done:
    free( before ); free( after ); free( kv ); free( old );
    return result;
}

enum game_profile_result game_profile_restore( const char *settings, const char *keys )
{
    struct targets t;
    struct snapshot *before = malloc( sizeof(*before) ), *after = malloc( sizeof(*after) );
    enum game_profile_result result = game_profile_recover( settings, keys );
    if (result != GAME_PROFILE_OK) goto done;
    result = GAME_PROFILE_IO;
    if (before && after && target_paths( &t, settings, keys ) && snapshot_capture( &t, before ) && snapshot_read( t.backup, after ))
        result = commit( &t, before, after, 1 );
 done:
    free( before ); free( after ); return result;
}

int game_profile_cover_path( const char *settings, char *out, size_t capacity )
{
    int n = snprintf( out, capacity, "%s.profile-cover.png", settings );
    return n >= 0 && (size_t)n < capacity;
}

enum game_profile_result game_profile_cheats_read( const char *settings, struct game_cheats *definitions,
                                                    struct launcher_kv *state )
{
    struct targets t;
    struct launcher_kv text;
    uint32_t size, exists;
    memset( definitions, 0, sizeof(*definitions) ); memset( state, 0, sizeof(*state) );
    if (!target_paths( &t, settings, settings ) || !read_local( t.paths[5], text.text, &size, &exists )) return GAME_PROFILE_IO;
    if (exists && !game_cheats_parse( text.text, definitions )) return GAME_PROFILE_INVALID;
    if (!read_local( t.paths[6], state->text, &size, &exists )) return GAME_PROFILE_IO;
    state->size = size;
    return GAME_PROFILE_OK;
}

enum game_profile_result game_profile_cheats_save( const char *settings, const char *keys,
                                                    const struct launcher_kv *state )
{
    struct targets t;
    struct snapshot *before = NULL, *after = NULL;
    struct game_cheats definitions;
    struct launcher_kv previous, clean;
    enum game_profile_result result = game_profile_recover( settings, keys );
    if (result != GAME_PROFILE_OK) return result;
    if ((result = game_profile_cheats_read( settings, &definitions, &previous )) != GAME_PROFILE_OK) return result;
    result = GAME_PROFILE_IO;
    if (!game_cheats_reconcile( &definitions, &definitions, state, 0, &clean ) ||
        !target_paths( &t, settings, keys ) || !(before = malloc( sizeof(*before) )) || !(after = malloc( sizeof(*after) ))) goto done;
    if (!snapshot_capture( &t, before )) goto done;
    *after = *before; after->sizes[6] = clean.size; after->exists[6] = 1;
    memcpy( after->data[6], clean.text, LAUNCHER_KV_MAX );
    result = commit( &t, before, after, 0 ); /* Keep the last profile-install backup. */
 done:
    free( before ); free( after ); return result;
}

const char *game_profile_error( enum game_profile_result result )
{
    switch (result)
    {
    case GAME_PROFILE_OK: return "适配包已应用。";
    case GAME_PROFILE_INVALID: return "适配包或绑定记录格式无效，请检查发布文件。";
    case GAME_PROFILE_IO: return "无法读取或保存适配配置，请检查存储空间、文件权限和配置大小。";
    case GAME_PROFILE_RECOVERY: return "上次适配安装未完成，且原配置未能恢复。请保持备份文件，修复存储问题后重试。";
    case GAME_PROFILE_OLD: return "当前适配包已是此更新源的最新版本，或本地版本更新。";
    case GAME_PROFILE_INCOMPATIBLE: return "此适配包需要更新版本的 Autorun，请先更新主程序。";
    case GAME_PROFILE_UNSUPPORTED: return "所选游戏程序不是此适配包支持的精确版本，未修改 EXE。";
    }
    return "适配包操作失败。";
}
