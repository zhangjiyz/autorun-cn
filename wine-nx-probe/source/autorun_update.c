#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <curl/curl.h>
#include <switch.h>

#include "autorun_update.h"

#define RELEASE_API "https://api.github.com/repos/" AUTORUN_DEFAULT_REPOSITORY "/releases/latest"
#define DOWNLOAD_PREFIX "https://github.com/" AUTORUN_DEFAULT_REPOSITORY "/releases/download/"
#define METADATA_MAX (1024u * 1024u)
#define ARCHIVE_MAX (512u * 1024u * 1024u)

static const struct autorun_update_source runtime_source = {
    RELEASE_API, DOWNLOAD_PREFIX, "autorun.zip", "release.zip", ARCHIVE_MAX, 0
};

static int source_component( const char *text, size_t len )
{
    if (!len || len > 100 || text[0] == '.' || text[len - 1] == '.') return 0;
    for (size_t i = 0; i < len; i++)
        if (!((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z') ||
              (text[i] >= '0' && text[i] <= '9') || text[i] == '-' || text[i] == '_' || text[i] == '.')) return 0;
    return 1;
}

int autorun_profile_source( struct autorun_update_source *source, const char *repository, const char *tag )
{
    const char *slash;
    if (!source || !repository || !(slash = strchr( repository, '/' )) ||
        !source_component( repository, slash - repository ) ||
        !source_component( slash + 1, strlen( slash + 1 ) ) ||
        (tag && tag[0] && !source_component( tag, strlen( tag ) ))) return 0;
    memset( source, 0, sizeof(*source) );
    snprintf( source->api, sizeof(source->api), "https://api.github.com/repos/%s/releases/%s%s",
              repository, tag && tag[0] ? "tags/" : "latest", tag ? tag : "" );
    snprintf( source->prefix, sizeof(source->prefix), "https://github.com/%s/releases/download/", repository );
    strcpy( source->asset, "autorun-profiles.zip" );
    strcpy( source->cache, "profiles.zip" );
    source->max_size = 16 * 1024 * 1024;
    return 1;
}

struct memory_buffer
{
    unsigned char *data;
    size_t size, capacity;
};

struct transfer_progress
{
    autorun_update_progress callback;
    void *opaque;
    int cancelled;
};

struct parser
{
    const unsigned char *p, *end;
    unsigned int depth;
};

struct asset
{
    char name[256], url[768], digest[65];
    unsigned long long size;
    unsigned int fields;
};

struct download_stream
{
    FILE *file;
    Sha256Context hash;
    unsigned long long size, expected;
    int io_error, too_large;
};

static int add_size( size_t a, size_t b, size_t *result )
{
    if (b > SIZE_MAX - a) return 0;
    *result = a + b;
    return 1;
}

static size_t receive_memory( void *data, size_t size, size_t count, void *opaque )
{
    struct memory_buffer *buffer = opaque;
    unsigned char *grown;
    size_t bytes, needed, capacity;

    if (count && size > SIZE_MAX / count) return 0;
    bytes = size * count;
    if (!add_size( buffer->size, bytes, &needed ) || needed > METADATA_MAX) return 0;
    if (!add_size( needed, 1, &needed )) return 0;
    if (needed > buffer->capacity)
    {
        capacity = buffer->capacity ? buffer->capacity : 16384;
        while (capacity < needed)
        {
            if (capacity > (METADATA_MAX + 1) / 2) { capacity = METADATA_MAX + 1; break; }
            capacity *= 2;
        }
        if (!(grown = realloc( buffer->data, capacity ))) return 0;
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy( buffer->data + buffer->size, data, bytes );
    buffer->size += bytes;
    buffer->data[buffer->size] = 0;
    return bytes;
}

static int transfer_callback( void *opaque, curl_off_t total, curl_off_t current,
                              curl_off_t upload_total, curl_off_t upload_current )
{
    struct transfer_progress *state = opaque;

    (void)upload_total;
    (void)upload_current;
    if (state->callback && state->callback( state->opaque,
            current > 0 ? (unsigned long long)current : 0,
            total > 0 ? (unsigned long long)total : 0 ))
        state->cancelled = 1;
    return state->cancelled;
}

static int set_https_options( CURL *curl )
{
    if (curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1L ) ||
        curl_easy_setopt( curl, CURLOPT_MAXREDIRS, 5L ) ||
        curl_easy_setopt( curl, CURLOPT_CONNECTTIMEOUT, 12L ) ||
        curl_easy_setopt( curl, CURLOPT_TIMEOUT, 900L ) ||
        curl_easy_setopt( curl, CURLOPT_LOW_SPEED_LIMIT, 1024L ) ||
        curl_easy_setopt( curl, CURLOPT_LOW_SPEED_TIME, 30L ) ||
        curl_easy_setopt( curl, CURLOPT_NOSIGNAL, 1L ) ||
        curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 1L ) ||
        curl_easy_setopt( curl, CURLOPT_SSL_VERIFYHOST, 2L ) ||
        curl_easy_setopt( curl, CURLOPT_USERAGENT, "Autorun/Updater" )) return 0;
#if LIBCURL_VERSION_NUM >= 0x075500
    if (curl_easy_setopt( curl, CURLOPT_PROTOCOLS_STR, "https" ) ||
        curl_easy_setopt( curl, CURLOPT_REDIR_PROTOCOLS_STR, "https" )) return 0;
#else
    if (curl_easy_setopt( curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS ) ||
        curl_easy_setopt( curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS )) return 0;
#endif
    return 1;
}

static void whitespace( struct parser *p )
{
    while (p->p < p->end && (*p->p == ' ' || *p->p == '\t' || *p->p == '\r' || *p->p == '\n')) p->p++;
}

static int hex4( const unsigned char *p, uint32_t *value )
{
    unsigned int i;
    uint32_t v = 0;

    for (i = 0; i < 4; i++)
    {
        unsigned char c = p[i];
        unsigned int n = c >= '0' && c <= '9' ? c - '0' :
                         c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                         c >= 'A' && c <= 'F' ? c - 'A' + 10 : 16;
        if (n == 16) return 0;
        v = v * 16 + n;
    }
    *value = v;
    return 1;
}

static int emit_utf8( uint32_t code, char *out, size_t size, size_t *used )
{
    unsigned char bytes[4];
    size_t count, i;

    if (!code) return 0;
    if (code <= 0x7f) { bytes[0] = code; count = 1; }
    else if (code <= 0x7ff) { bytes[0] = 0xc0 | (code >> 6); bytes[1] = 0x80 | (code & 63); count = 2; }
    else if (code <= 0xffff) { bytes[0] = 0xe0 | (code >> 12); bytes[1] = 0x80 | ((code >> 6) & 63); bytes[2] = 0x80 | (code & 63); count = 3; }
    else if (code <= 0x10ffff) { bytes[0] = 0xf0 | (code >> 18); bytes[1] = 0x80 | ((code >> 12) & 63); bytes[2] = 0x80 | ((code >> 6) & 63); bytes[3] = 0x80 | (code & 63); count = 4; }
    else return 0;
    if (*used + count >= size) return 0;
    for (i = 0; i < count; i++) out[(*used)++] = bytes[i];
    return 1;
}

static int json_string( struct parser *p, char *out, size_t size )
{
    size_t used = 0;

    whitespace( p );
    if (!size || p->p == p->end || *p->p++ != '"') return 0;
    while (p->p < p->end)
    {
        uint32_t code;
        unsigned char c = *p->p++;

        if (c == '"') { out[used] = 0; return 1; }
        if (c < 0x20) return 0;
        if (c != '\\')
        {
            if (used + 1 >= size) return 0;
            out[used++] = c;
            continue;
        }
        if (p->p == p->end) return 0;
        c = *p->p++;
        if (c == '"' || c == '\\' || c == '/') code = c;
        else if (c == 'b') code = '\b';
        else if (c == 'f') code = '\f';
        else if (c == 'n') code = '\n';
        else if (c == 'r') code = '\r';
        else if (c == 't') code = '\t';
        else if (c == 'u')
        {
            uint32_t low;
            if (p->end - p->p < 4 || !hex4( p->p, &code )) return 0;
            p->p += 4;
            if (code >= 0xd800 && code <= 0xdbff)
            {
                if (p->end - p->p < 6 || p->p[0] != '\\' || p->p[1] != 'u' ||
                    !hex4( p->p + 2, &low ) || low < 0xdc00 || low > 0xdfff) return 0;
                p->p += 6;
                code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
            }
            else if (code >= 0xdc00 && code <= 0xdfff) return 0;
        }
        else return 0;
        if (!emit_utf8( code, out, size, &used )) return 0;
    }
    return 0;
}

static int skip_value( struct parser *p );

static int skip_string( struct parser *p )
{
    whitespace( p );
    if (p->p == p->end || *p->p++ != '"') return 0;
    while (p->p < p->end)
    {
        unsigned char c = *p->p++;
        uint32_t code, low;

        if (c == '"') return 1;
        if (c < 0x20) return 0;
        if (c != '\\') continue;
        if (p->p == p->end) return 0;
        c = *p->p++;
        if (strchr( "\"\\/bfnrt", c )) continue;
        if (c != 'u' || p->end - p->p < 4 || !hex4( p->p, &code )) return 0;
        p->p += 4;
        if (code >= 0xd800 && code <= 0xdbff)
        {
            if (p->end - p->p < 6 || p->p[0] != '\\' || p->p[1] != 'u' ||
                !hex4( p->p + 2, &low ) || low < 0xdc00 || low > 0xdfff) return 0;
            p->p += 6;
        }
        else if (code >= 0xdc00 && code <= 0xdfff) return 0;
    }
    return 0;
}

static int consume( struct parser *p, unsigned char c )
{
    whitespace( p );
    if (p->p == p->end || *p->p != c) return 0;
    p->p++;
    return 1;
}

static int skip_compound( struct parser *p, unsigned char close )
{
    whitespace( p );
    if (p->p < p->end && *p->p == close) { p->p++; return 1; }
    for (;;)
    {
        if (close == '}' && (!skip_string( p ) || !consume( p, ':' ))) return 0;
        if (!skip_value( p )) return 0;
        whitespace( p );
        if (p->p < p->end && *p->p == close) { p->p++; return 1; }
        if (p->p == p->end || *p->p++ != ',') return 0;
    }
}

static int skip_value( struct parser *p )
{
    const unsigned char *start;
    int result;

    whitespace( p );
    if (p->p == p->end) return 0;
    if (*p->p == '"') return skip_string( p );
    if (*p->p == '{' || *p->p == '[')
    {
        unsigned char close = *p->p++ == '{' ? '}' : ']';
        if (++p->depth > 32) { p->depth--; return 0; }
        result = skip_compound( p, close );
        p->depth--;
        return result;
    }
    start = p->p;
    while (p->p < p->end && !strchr( " \t\r\n,]}", *p->p )) p->p++;
    if (p->p == start) return 0;
    if ((size_t)(p->p - start) == 4 && (!memcmp( start, "true", 4 ) || !memcmp( start, "null", 4 ))) return 1;
    if ((size_t)(p->p - start) == 5 && !memcmp( start, "false", 5 )) return 1;
    if (*start == '-') start++;
    if (start == p->p) return 0;
    if (*start == '0') start++;
    else { if (!isdigit( *start )) return 0; while (start < p->p && isdigit( *start )) start++; }
    if (start < p->p && *start == '.') { start++; if (start == p->p || !isdigit( *start )) return 0; while (start < p->p && isdigit( *start )) start++; }
    if (start < p->p && (*start == 'e' || *start == 'E')) { start++; if (start < p->p && (*start == '+' || *start == '-')) start++; if (start == p->p || !isdigit( *start )) return 0; while (start < p->p && isdigit( *start )) start++; }
    return start == p->p;
}

static int json_bool( struct parser *p, int *value )
{
    whitespace( p );
    if (p->end - p->p >= 4 && !memcmp( p->p, "true", 4 )) { p->p += 4; *value = 1; return 1; }
    if (p->end - p->p >= 5 && !memcmp( p->p, "false", 5 )) { p->p += 5; *value = 0; return 1; }
    return 0;
}

static int json_null( struct parser *p )
{
    whitespace( p );
    if (p->end - p->p < 4 || memcmp( p->p, "null", 4 )) return 0;
    p->p += 4;
    return 1;
}

static int json_uint( struct parser *p, unsigned long long *value )
{
    unsigned long long v = 0;
    const unsigned char *start;

    whitespace( p );
    start = p->p;
    if (p->p == p->end || !isdigit( *p->p )) return 0;
    if (*p->p == '0' && p->p + 1 < p->end && isdigit( p->p[1] )) return 0;
    while (p->p < p->end && isdigit( *p->p ))
    {
        unsigned int n = *p->p++ - '0';
        if (v > (UINT64_MAX - n) / 10) return 0;
        v = v * 10 + n;
    }
    if (p->p == start || (p->p < p->end && (*p->p == '.' || *p->p == 'e' || *p->p == 'E'))) return 0;
    *value = v;
    return 1;
}

static int digest( const char *text, char out[65] )
{
    size_t i;

    if (strncmp( text, "sha256:", 7 ) || strlen( text + 7 ) != 64) return 0;
    for (i = 0; i < 64; i++)
    {
        if (!isxdigit( (unsigned char)text[i + 7] )) return 0;
        out[i] = tolower( (unsigned char)text[i + 7] );
    }
    out[64] = 0;
    return 1;
}

static int parse_asset( struct parser *p, struct asset *asset )
{
    char key[64], hash[96];

    memset( asset, 0, sizeof(*asset) );
    if (!consume( p, '{' )) return 0;
    whitespace( p );
    if (p->p < p->end && *p->p == '}') { p->p++; return 1; }
    for (;;)
    {
        if (!json_string( p, key, sizeof(key) ) || !consume( p, ':' )) return 0;
        if (!strcmp( key, "name" )) { if ((asset->fields & 1) || !json_string( p, asset->name, sizeof(asset->name) )) return 0; asset->fields |= 1; }
        else if (!strcmp( key, "browser_download_url" )) { if ((asset->fields & 2) || !json_string( p, asset->url, sizeof(asset->url) )) return 0; asset->fields |= 2; }
        else if (!strcmp( key, "size" )) { if ((asset->fields & 4) || !json_uint( p, &asset->size )) return 0; asset->fields |= 4; }
        else if (!strcmp( key, "digest" ))
        {
            if (asset->fields & 8) return 0;
            whitespace( p );
            if (p->end - p->p >= 4 && !memcmp( p->p, "null", 4 )) p->p += 4;
            else if (!json_string( p, hash, sizeof(hash) ) || !digest( hash, asset->digest )) return 0;
            asset->fields |= 8;
        }
        else if (!skip_value( p )) return 0;
        whitespace( p );
        if (p->p < p->end && *p->p == '}') { p->p++; return 1; }
        if (p->p == p->end || *p->p++ != ',') return 0;
    }
}

static int zip_name( const char *name )
{
    size_t length = strlen( name );
    return length > 4 && !strcasecmp( name + length - 4, ".zip" );
}

static int canonical_asset( const struct asset *asset, const char *tag )
{
    char tagged[160];
    int length = snprintf( tagged, sizeof(tagged), "autorun-%s.zip", tag );
    return !strcasecmp( asset->name, "autorun.zip" ) ||
           (length > 0 && (size_t)length < sizeof(tagged) && !strcasecmp( asset->name, tagged ));
}

static int official_url( const struct autorun_update_source *source, const char *url )
{
    return source->prefix[0] && !strncasecmp( url, source->prefix, strlen( source->prefix ) );
}

static int parse_assets( struct parser *p, const char *tag, struct asset *chosen,
                         const struct autorun_update_source *source )
{
    struct asset only = {0}, preferred = {0}, asset;
    unsigned int zip_count = 0, preferred_count = 0;

    if (!consume( p, '[' )) return 0;
    whitespace( p );
    if (p->p < p->end && *p->p == ']') { p->p++; return 0; }
    for (;;)
    {
        if (!parse_asset( p, &asset )) return 0;
        if (asset.fields == 15 && asset.digest[0] && asset.size && asset.size <= source->max_size && zip_name( asset.name ) && official_url( source, asset.url ) &&
            (source->asset[0] ? !strcmp( source->asset, asset.name ) : strcasecmp( asset.name, "autorun-profiles.zip" )))
        {
            zip_count++;
            only = asset;
            if (source->asset[0] || canonical_asset( &asset, tag )) { preferred_count++; preferred = asset; }
        }
        whitespace( p );
        if (p->p < p->end && *p->p == ']') { p->p++; break; }
        if (p->p == p->end || *p->p++ != ',') return 0;
    }
    if (preferred_count == 1) *chosen = preferred;
    else if (!preferred_count && zip_count == 1) *chosen = only;
    else return 0;
    return 1;
}

static int release_tag( const unsigned char *data, size_t size, char *tag, size_t tag_size )
{
    struct parser p = {data, data + size, 0};
    char key[64];
    int found = 0;

    if (!consume( &p, '{' )) return 0;
    for (;;)
    {
        whitespace( &p );
        if (p.p < p.end && *p.p == '}') { p.p++; break; }
        if (!json_string( &p, key, sizeof(key) ) || !consume( &p, ':' )) return 0;
        if (!strcmp( key, "tag_name" ))
        {
            if (found || !json_string( &p, tag, tag_size ) || !tag[0]) return 0;
            found = 1;
        }
        else if (!skip_value( &p )) return 0;
        whitespace( &p );
        if (p.p < p.end && *p.p == '}') { p.p++; break; }
        if (p.p == p.end || *p.p++ != ',') return 0;
    }
    whitespace( &p );
    return found && p.p == p.end;
}

static int parse_release( const unsigned char *data, size_t size, struct autorun_release *release,
                          const struct autorun_update_source *source )
{
    struct parser p;
    struct asset asset = {0};
    char key[64], tag[64];
    int draft = -1, prerelease = -1;
    unsigned int fields = 0;

    if (!data || !size || !release_tag( data, size, tag, sizeof(tag) )) return 0;
    p = (struct parser){data, data + size, 0};
    memset( release, 0, sizeof(*release) );
    if (!consume( &p, '{' )) return 0;
    for (;;)
    {
        whitespace( &p );
        if (p.p < p.end && *p.p == '}') { p.p++; break; }
        if (!json_string( &p, key, sizeof(key) ) || !consume( &p, ':' )) return 0;
        if (!strcmp( key, "tag_name" )) { if ((fields & 1) || !json_string( &p, release->tag, sizeof(release->tag) ) || strcmp( release->tag, tag )) return 0; fields |= 1; }
        else if (!strcmp( key, "name" )) { if (fields & 2) return 0; whitespace( &p ); if (!json_null( &p ) && !json_string( &p, release->name, sizeof(release->name) )) return 0; fields |= 2; }
        else if (!strcmp( key, "published_at" )) { if ((fields & 4) || !json_string( &p, release->published, sizeof(release->published) )) return 0; fields |= 4; }
        else if (!strcmp( key, "body" )) { if (fields & 8) return 0; whitespace( &p ); if (!json_null( &p ) && !json_string( &p, release->notes, sizeof(release->notes) )) return 0; fields |= 8; }
        else if (!strcmp( key, "draft" )) { if ((fields & 16) || !json_bool( &p, &draft )) return 0; fields |= 16; }
        else if (!strcmp( key, "prerelease" )) { if ((fields & 32) || !json_bool( &p, &prerelease )) return 0; fields |= 32; }
        else if (!strcmp( key, "assets" )) { if ((fields & 64) || !parse_assets( &p, tag, &asset, source )) return 0; fields |= 64; }
        else if (!skip_value( &p )) return 0;
        whitespace( &p );
        if (p.p < p.end && *p.p == '}') { p.p++; break; }
        if (p.p == p.end || *p.p++ != ',') return 0;
    }
    whitespace( &p );
    if (p.p != p.end || fields != 127 || draft || prerelease || !release->tag[0] || !asset.digest[0]) return 0;
    if (!release->name[0]) snprintf( release->name, sizeof(release->name), "%s", release->tag );
    memcpy( release->url, asset.url, sizeof(release->url) );
    memcpy( release->digest, asset.digest, sizeof(release->digest) );
    release->size = asset.size;
    return 1;
}

static enum autorun_update_result request_metadata( const struct autorun_update_source *source, struct memory_buffer *body,
        autorun_update_progress callback, void *opaque )
{
    CURL *curl;
    CURLcode code;
    struct curl_slist *headers = NULL;
    struct transfer_progress progress = {callback, opaque, 0};
    long status = 0;

    memset( body, 0, sizeof(*body) );
    if (curl_global_init( CURL_GLOBAL_DEFAULT ) != CURLE_OK || !(curl = curl_easy_init())) return AUTORUN_UPDATE_NETWORK;
    headers = curl_slist_append( headers, "Accept: application/vnd.github+json" );
    headers = curl_slist_append( headers, "X-GitHub-Api-Version: 2022-11-28" );
    if (!headers || !set_https_options( curl ) ||
        curl_easy_setopt( curl, CURLOPT_TIMEOUT, source->timeout_seconds ? source->timeout_seconds : 45L ) ||
        curl_easy_setopt( curl, CURLOPT_URL, source->api ) ||
        curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers ) ||
        curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, receive_memory ) ||
        curl_easy_setopt( curl, CURLOPT_WRITEDATA, body ) ||
        curl_easy_setopt( curl, CURLOPT_NOPROGRESS, 0L ) ||
        curl_easy_setopt( curl, CURLOPT_XFERINFOFUNCTION, transfer_callback ) ||
        curl_easy_setopt( curl, CURLOPT_XFERINFODATA, &progress )) code = CURLE_FAILED_INIT;
    else code = curl_easy_perform( curl );
    curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &status );
    curl_slist_free_all( headers );
    curl_easy_cleanup( curl );
    if (progress.cancelled) return AUTORUN_UPDATE_CANCELLED;
    if (code != CURLE_OK) return AUTORUN_UPDATE_NETWORK;
    if (status == 404) return AUTORUN_UPDATE_NOT_FOUND;
    if (status < 200 || status >= 300) return AUTORUN_UPDATE_NETWORK;
    return AUTORUN_UPDATE_OK;
}

enum autorun_update_result autorun_update_check_source( const struct autorun_update_source *source,
        struct autorun_release *release,
        autorun_update_progress callback, void *opaque )
{
    struct memory_buffer body;
    enum autorun_update_result result;

    if (!source || !release) return AUTORUN_UPDATE_INVALID;
    result = request_metadata( source, &body, callback, opaque );
    if (result == AUTORUN_UPDATE_OK && !parse_release( body.data, body.size, release, source )) result = AUTORUN_UPDATE_INVALID;
    free( body.data );
    return result;
}

static size_t receive_file( void *data, size_t size, size_t count, void *opaque )
{
    struct download_stream *stream = opaque;
    size_t bytes;

    if (count && size > SIZE_MAX / count) { stream->too_large = 1; return 0; }
    bytes = size * count;
    if (bytes > ARCHIVE_MAX - stream->size || bytes > stream->expected - stream->size)
    { stream->too_large = 1; return 0; }
    if (fwrite( data, 1, bytes, stream->file ) != bytes) { stream->io_error = 1; return 0; }
    sha256ContextUpdate( &stream->hash, data, bytes );
    stream->size += bytes;
    return bytes;
}

static int make_updates( const char *runtime_dir, char *folder, size_t size )
{
    struct stat st;
    int length;

    if (!runtime_dir || !runtime_dir[0]) return 0;
    length = snprintf( folder, size, "%s%supdates", runtime_dir,
                       runtime_dir[strlen(runtime_dir) - 1] == '/' ? "" : "/" );
    if (length <= 0 || (size_t)length >= size) return 0;
    if (!mkdir( folder, 0777 )) return 1;
    return errno == EEXIST && !lstat( folder, &st ) && S_ISDIR( st.st_mode ) && !S_ISLNK( st.st_mode );
}

static int digest_matches( Sha256Context *context, const char *expected )
{
    static const char digits[] = "0123456789abcdef";
    unsigned char hash[SHA256_HASH_SIZE];
    char text[SHA256_HASH_SIZE * 2 + 1];
    size_t i;

    sha256ContextGetHash( context, hash );
    for (i = 0; i < sizeof(hash); i++)
    {
        text[i * 2] = digits[hash[i] >> 4];
        text[i * 2 + 1] = digits[hash[i] & 15];
    }
    text[64] = 0;
    return !strcasecmp( text, expected );
}

enum autorun_update_result autorun_update_download_source( const struct autorun_update_source *source,
        const char *runtime_dir,
        const struct autorun_release *release, char *path, size_t path_size,
        autorun_update_progress callback, void *opaque )
{
    char folder[768], part[800], final[800];
    struct transfer_progress progress = {callback, opaque, 0};
    struct download_stream stream;
    CURL *curl = NULL;
    CURLcode code = CURLE_FAILED_INIT;
    long status = 0;
    enum autorun_update_result result = AUTORUN_UPDATE_NETWORK;
    if (!source || !source_component( source->cache, strlen( source->cache ) ) ||
        !release || !path || !path_size || !official_url( source, release->url ) ||
        !release->size || release->size > source->max_size || release->size > ARCHIVE_MAX || strlen( release->digest ) != 64)
        return AUTORUN_UPDATE_INVALID;
    path[0] = 0;
    for (size_t i = 0; i < 64; i++) if (!isxdigit( (unsigned char)release->digest[i] )) return AUTORUN_UPDATE_INVALID;
    if (!make_updates( runtime_dir, folder, sizeof(folder) )) return AUTORUN_UPDATE_IO;
    if ((size_t)snprintf( part, sizeof(part), "%s/%s.part", folder, source->cache ) >= sizeof(part) ||
        (size_t)snprintf( final, sizeof(final), "%s/%s", folder, source->cache ) >= sizeof(final) ||
        strlen( final ) >= path_size)
        return AUTORUN_UPDATE_INVALID;
    remove( part );
    memset( &stream, 0, sizeof(stream) );
    stream.expected = release->size;
    sha256ContextCreate( &stream.hash );
    if (!(stream.file = fopen( part, "wb" ))) return AUTORUN_UPDATE_IO;
    if (curl_global_init( CURL_GLOBAL_DEFAULT ) == CURLE_OK && (curl = curl_easy_init()) &&
        set_https_options( curl ) &&
        !curl_easy_setopt( curl, CURLOPT_TIMEOUT, source->timeout_seconds ? source->timeout_seconds : 900L ) &&
        !curl_easy_setopt( curl, CURLOPT_URL, release->url ) &&
        !curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, receive_file ) &&
        !curl_easy_setopt( curl, CURLOPT_WRITEDATA, &stream ) &&
        !curl_easy_setopt( curl, CURLOPT_NOPROGRESS, 0L ) &&
        !curl_easy_setopt( curl, CURLOPT_XFERINFOFUNCTION, transfer_callback ) &&
        !curl_easy_setopt( curl, CURLOPT_XFERINFODATA, &progress ))
        code = curl_easy_perform( curl );
    if (curl) { curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &status ); curl_easy_cleanup( curl ); }
    if (fflush( stream.file )) stream.io_error = 1;
    if (fsync( fileno( stream.file ) )) stream.io_error = 1;
    if (fclose( stream.file )) stream.io_error = 1;
    stream.file = NULL;
    if (progress.cancelled) result = AUTORUN_UPDATE_CANCELLED;
    else if (stream.io_error) result = AUTORUN_UPDATE_IO;
    else if (stream.too_large) result = AUTORUN_UPDATE_INVALID;
    else if (code != CURLE_OK || status < 200 || status >= 300) result = status == 404 ? AUTORUN_UPDATE_NOT_FOUND : AUTORUN_UPDATE_NETWORK;
    else if (stream.size != release->size) result = AUTORUN_UPDATE_INVALID;
    else if (!digest_matches( &stream.hash, release->digest )) result = AUTORUN_UPDATE_HASH;
    else
    {
        remove( final );
        result = rename( part, final ) ? AUTORUN_UPDATE_IO : AUTORUN_UPDATE_OK;
        if (result == AUTORUN_UPDATE_OK) snprintf( path, path_size, "%s", final );
    }
    if (result != AUTORUN_UPDATE_OK) remove( part );
    return result;
}

int autorun_https_url( const char *url )
{
    if (!url || strncmp( url, "https://", 8 ) || !url[8] || url[8] == '/' || strlen( url ) >= 768) return 0;
    for (const unsigned char *p = (const void *)url; *p; p++) if (*p <= 32 || *p >= 127 || *p == '\\' || *p == '#' || *p == '@') return 0;
    return 1;
}

enum autorun_update_result autorun_update_text( const char *url, char **text, size_t *size,
        long timeout_seconds, autorun_update_progress progress, void *opaque )
{
    struct autorun_update_source source = {0};
    struct memory_buffer body = {0};
    *text = NULL; *size = 0;
    if (!autorun_https_url( url ) || strlen( url ) >= sizeof(source.api)) return AUTORUN_UPDATE_INVALID;
    strcpy( source.api, url ); source.timeout_seconds = timeout_seconds;
    enum autorun_update_result result = request_metadata( &source, &body, progress, opaque );
    if (result == AUTORUN_UPDATE_OK && (!body.size || memchr( body.data, 0, body.size ))) result = AUTORUN_UPDATE_INVALID;
    if (result == AUTORUN_UPDATE_OK) { *text = (char *)body.data; *size = body.size; }
    else free( body.data );
    return result;
}

int autorun_file_matches( const char *path, unsigned long long size, const char *digest )
{
    unsigned char buffer[16384];
    Sha256Context hash; unsigned long long used = 0;
    size_t n;
    FILE *file = fopen( path, "rb" );
    if (!file) return 0;
    sha256ContextCreate( &hash );
    while ((n = fread( buffer, 1, sizeof(buffer), file )))
    {
        used += n;
        if (used > size) { fclose( file ); return 0; }
        sha256ContextUpdate( &hash, buffer, n );
    }
    int ok = used == size && !ferror( file ) && digest_matches( &hash, digest );
    fclose( file ); return ok;
}

enum autorun_update_result autorun_update_file( const char *url, unsigned long long size, const char *digest,
        const char *root, char *path, size_t capacity, long timeout_seconds,
        autorun_update_progress progress, void *opaque )
{
    struct autorun_update_source source = {0};
    struct autorun_release release = {0};
    if (!autorun_https_url( url ) || strlen( digest ) != 64) return AUTORUN_UPDATE_INVALID;
    strcpy( source.prefix, "https://" ); strcpy( source.cache, "profile-selected.zip" );
    source.max_size = 16 * 1024 * 1024; source.timeout_seconds = timeout_seconds;
    strcpy( release.url, url ); strcpy( release.digest, digest ); release.size = size;
    return autorun_update_download_source( &source, root, &release, path, capacity, progress, opaque );
}

enum autorun_update_result autorun_update_check( struct autorun_release *release,
        autorun_update_progress callback, void *opaque )
{
    return autorun_update_check_source( &runtime_source, release, callback, opaque );
}

enum autorun_update_result autorun_update_download( const char *runtime_dir,
        const struct autorun_release *release, char *path, size_t path_size,
        autorun_update_progress callback, void *opaque )
{
    return autorun_update_download_source( &runtime_source, runtime_dir, release, path, path_size, callback, opaque );
}

const char *autorun_update_error( enum autorun_update_result result )
{
    switch (result)
    {
    case AUTORUN_UPDATE_OK: return "The update is ready.";
    case AUTORUN_UPDATE_CANCELLED: return "The update was cancelled.";
    case AUTORUN_UPDATE_NETWORK: return "Could not download the update from GitHub.";
    case AUTORUN_UPDATE_NOT_FOUND: return "No official Autorun update was found.";
    case AUTORUN_UPDATE_INVALID: return "GitHub returned an invalid Autorun release.";
    case AUTORUN_UPDATE_IO: return "The update could not be written to the SD card.";
    case AUTORUN_UPDATE_HASH: return "The update failed SHA-256 verification.";
    }
    return "The update failed.";
}
