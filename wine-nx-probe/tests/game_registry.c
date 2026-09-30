/* Exercise the production NT calls against the actual Horizon registry tree. */
#include <assert.h>
#include <iconv.h>
#include <stdlib.h>
#include <stdio.h>
#include "../source/game_registry.c"
#include "../../dlls/ntdll/unix/horizon_registry.h"

static struct horizon_reg registry;
static unsigned int writes, flushes, handles;
static NTSTATUS query_error, write_error, flush_error, create_error;
static long long now(void) { return 1234; }
static void signal_event(void *event) { (void)event; }

NTSTATUS WINAPI NtCreateKey( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                            ULONG title, const UNICODE_STRING *class, ULONG options, ULONG *disposition )
{
    struct horizon_reg_key *key;
    unsigned int status;
    (void)title; (void)class; (void)disposition;
    assert( access == (KEY_CREATE_SUB_KEY | KEY_QUERY_VALUE | KEY_SET_VALUE) );
    assert( !options && !attr->RootDirectory );
    if (create_error) return create_error;
    status = horizon_reg_create( &registry, NULL, attr->ObjectName->Buffer, attr->ObjectName->Length,
                                 attr->Attributes, options, NULL, 0, &key );
    if (status && status != HORIZON_REG_NAME_EXISTS) return status;
    *handle = key;
    handles++;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI NtClose( HANDLE handle )
{
    assert( handles ); handles--;
    horizon_reg_release( &registry, handle );
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI NtQueryValueKey( HANDLE key, const UNICODE_STRING *name, KEY_VALUE_INFORMATION_CLASS class,
                                void *buffer, DWORD capacity, DWORD *size )
{
    KEY_VALUE_PARTIAL_INFORMATION *info = buffer;
    unsigned int index;
    struct horizon_reg_value *value;
    assert( class == KeyValuePartialInformation );
    if (query_error) return query_error;
    value = horizon_reg_find_value( key, name->Buffer, name->Length, &index );
    if (!value) return STATUS_OBJECT_NAME_NOT_FOUND;
    *size = offsetof(KEY_VALUE_PARTIAL_INFORMATION, Data) + value->len;
    if (*size > capacity) return STATUS_BUFFER_TOO_SMALL;
    info->Type = value->type; info->DataLength = value->len;
    memcpy( info->Data, value->data, value->len );
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI NtSetValueKey( HANDLE key, const UNICODE_STRING *name, ULONG title, ULONG type,
                              const void *data, ULONG size )
{
    (void)title;
    if (write_error) return write_error;
    writes++;
    return horizon_reg_set_value( &registry, key, name->Buffer, name->Length, type, data, size );
}

NTSTATUS WINAPI NtFlushKey( HANDLE key )
{
    assert( key ); flushes++;
    return flush_error;
}

NTSTATUS WINAPI RtlUTF8ToUnicodeN( WCHAR *out, DWORD capacity, DWORD *written, const char *in, DWORD bytes )
{
    iconv_t converter = iconv_open( "UTF-16LE", "UTF-8" );
    char *source = (char *)in, *destination = (char *)out;
    size_t remaining = bytes, available = capacity;
    assert( converter != (iconv_t)-1 );
    size_t result = iconv( converter, &source, &remaining, &destination, &available );
    iconv_close( converter );
    *written = capacity - available;
    return result == (size_t)-1 ? STATUS_INVALID_PARAMETER : STATUS_SUCCESS;
}

static struct horizon_reg_key *biko_key(void)
{
    WCHAR path[128]; DWORD bytes;
    struct horizon_reg_key *key;
    const char *text = "\\Registry\\Machine\\Software\\illusion\\Bikou3_DVD";
    assert( !RtlUTF8ToUnicodeN( path, sizeof(path), &bytes, text, strlen(text) ) );
    assert( !horizon_reg_open( &registry, NULL, path, bytes, 0, &key ) );
    return key;
}

static void expect( const char *text )
{
    static const WCHAR name[] = {'I','N','S','T','A','L','L','D','I','R'};
    WCHAR expected[1024]; DWORD size;
    unsigned int index;
    struct horizon_reg_key *key = biko_key();
    struct horizon_reg_value *value = horizon_reg_find_value( key, name, sizeof(name), &index );
    assert( !RtlUTF8ToUnicodeN( expected, sizeof(expected), &size, text, strlen(text) + 1 ) );
    assert( value && value->type == REG_SZ && value->len == size && !memcmp( value->data, expected, size ) );
    horizon_reg_release( &registry, key );
}

int main(void)
{
    const char *path = "Software/illusion/Bikou3_DVD";
    int changed;
    assert( horizon_reg_init( &registry, now, signal_event ) );
    /* Entire missing subtree, with non-volatile parents and an exact final slash. */
    assert( !game_registry_ensure_install_dir( path, "C:\\Biko3\\Biko_DVD.exe", &changed ) && changed );
    expect( "C:\\Biko3\\" ); assert( writes == 1 && flushes == 1 && !handles );
    assert( !game_registry_ensure_install_dir( path, "C:\\Biko3\\renamed.exe", &changed ) && !changed );
    assert( writes == 1 && !handles );
    /* Another value under the same game key survives moving the game. */
    struct horizon_reg_key *key = biko_key();
    static const WCHAR other[] = {'O','t','h','e','r'};
    unsigned int sentinel = 42, index;
    assert( !horizon_reg_set_value( &registry, key, other, sizeof(other), REG_DWORD, &sentinel, sizeof(sentinel) ) );
    assert( !game_registry_ensure_install_dir( path, "D:\\games\\尾行3\\_dvd.exe", &changed ) && changed );
    expect( "D:\\games\\尾行3\\" );
    struct horizon_reg_value *value = horizon_reg_find_value( key, other, sizeof(other), &index );
    assert( value && value->type == REG_DWORD && value->len == 4 && !memcmp(value->data, &sentinel, 4) );
    horizon_reg_release( &registry, key );
    assert( !game_registry_ensure_install_dir( path, "C:\\Game.exe", &changed ) && changed );
    expect( "C:\\" );
    /* Wrong type and an oversized stale value both need replacement. */
    key = biko_key();
    static const WCHAR install[] = {'I','N','S','T','A','L','L','D','I','R'};
    assert( !horizon_reg_set_value( &registry, key, install, sizeof(install), REG_DWORD, &sentinel, 4 ) );
    assert( !game_registry_ensure_install_dir( path, "C:/Biko3/Game.exe", &changed ) && changed );
    expect( "C:\\Biko3\\" );
    char large[4096] = {0};
    assert( !horizon_reg_set_value( &registry, key, install, sizeof(install), REG_SZ, large, sizeof(large) ) );
    assert( !game_registry_ensure_install_dir( path, "C:\\Biko3\\Game.exe", &changed ) && changed );
    horizon_reg_release( &registry, key );
    expect( "C:\\Biko3\\" );
    /* A failed registry read must not silently replace an inaccessible value. */
    unsigned int before = writes;
    query_error = STATUS_ACCESS_DENIED;
    assert( game_registry_ensure_install_dir( path, "C:\\new\\Game.exe", &changed ) == (uint32_t)query_error );
    assert( !changed && writes == before && !handles ); query_error = 0;
    write_error = STATUS_ACCESS_DENIED;
    assert( game_registry_ensure_install_dir( path, "C:\\new\\Game.exe", &changed ) == (uint32_t)write_error );
    assert( !changed && !handles ); write_error = 0;
    flush_error = STATUS_ACCESS_DENIED;
    assert( game_registry_ensure_install_dir( path, "C:\\new\\Game.exe", &changed ) == (uint32_t)flush_error && changed );
    assert( !handles ); flush_error = 0;
    assert( !game_registry_ensure_install_dir( path, "C:\\new\\Game.exe", &changed ) && !changed );
    create_error = STATUS_ACCESS_DENIED;
    assert( game_registry_ensure_install_dir( path, "C:\\Game.exe", &changed ) == (uint32_t)create_error );
    assert( !handles ); create_error = 0;
    for (const char **p = (const char *[]){"", "Software", "Software/", "System/foo", "Software//foo",
         "Software/../foo", "Software/a\\b", "Software/a:b", "Software/a/", "Software/a.", "Software/中文", NULL}; *p; p++)
        assert( game_registry_ensure_install_dir( *p, "C:\\Game.exe", &changed ) == (uint32_t)STATUS_INVALID_PARAMETER );
    assert( game_registry_ensure_install_dir( path, "relative.exe", &changed ) == (uint32_t)STATUS_INVALID_PARAMETER );
    horizon_reg_release( &registry, registry.root );
    puts( "game registry repair: OK" );
    return 0;
}
