#include "game_registry.h"
#include <stddef.h>
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"

uint32_t game_registry_ensure_install_dir( const char *path, const char *exe, int *changed )
{
    static const char prefix[] = "\\Registry\\Machine\\";
    static const WCHAR value_name[] = {'I','N','S','T','A','L','L','D','I','R'};
    WCHAR key_path[256], directory[1024];
    char utf8[1024];
    struct { KEY_VALUE_PARTIAL_INFORMATION info; WCHAR extra[1024]; } current;
    UNICODE_STRING name = {0}, value = {sizeof(value_name), sizeof(value_name), (WCHAR *)value_name};
    OBJECT_ATTRIBUTES attr;
    HANDLE key = NULL, next;
    NTSTATUS status;
    DWORD bytes;
    ULONG size;
    const char *slash;
    size_t length, dir_length;

    *changed = 0;
    if (!game_registry_valid_key( path ) || !exe ||
        !((exe[0] >= 'A' && exe[0] <= 'Z') || (exe[0] >= 'a' && exe[0] <= 'z')) ||
        exe[1] != ':' || (exe[2] != '\\' && exe[2] != '/')) return STATUS_INVALID_PARAMETER;
    slash = strrchr( exe, '\\' );
    const char *forward = strrchr( exe, '/' );
    if (forward && (!slash || forward > slash)) slash = forward;
    if (!slash || !slash[1]) return STATUS_INVALID_PARAMETER;
    dir_length = slash - exe + 1; /* Includes the final separator required by Biko3. */
    if (dir_length >= sizeof(utf8)) return STATUS_NAME_TOO_LONG;
    memcpy( utf8, exe, dir_length );
    for (size_t i = 0; i < dir_length; i++) if (utf8[i] == '/') utf8[i] = '\\';
    status = RtlUTF8ToUnicodeN( directory, sizeof(directory) - sizeof(WCHAR), &bytes, utf8, dir_length );
    if (status) return status;
    directory[bytes / sizeof(WCHAR)] = 0;
    bytes += sizeof(WCHAR);

    length = strlen( prefix );
    for (size_t i = 0; i < length; i++) key_path[i] = (unsigned char)prefix[i];
    for (size_t i = 0; path[i]; i++) key_path[length++] = path[i] == '/' ? '\\' : (unsigned char)path[i];
    key_path[length] = 0;
    name.Buffer = key_path;
    name.MaximumLength = sizeof(key_path);
    /* NtCreateKey cannot create several missing parents in one call. */
    for (size_t i = 1; i <= length; i++)
    {
        WCHAR end = key_path[i];
        if (end && end != '\\') continue;
        key_path[i] = 0;
        name.Length = i * sizeof(WCHAR);
        InitializeObjectAttributes( &attr, &name, OBJ_CASE_INSENSITIVE, NULL, NULL );
        status = NtCreateKey( &next, KEY_CREATE_SUB_KEY | KEY_QUERY_VALUE | KEY_SET_VALUE,
                              &attr, 0, NULL, REG_OPTION_NON_VOLATILE, NULL );
        key_path[i] = end;
        if (status) return status;
        if (!end) key = next;
        else NtClose( next );
    }
    status = NtQueryValueKey( key, &value, KeyValuePartialInformation, &current, sizeof(current), &size );
    if (!status && current.info.Type == REG_SZ && current.info.DataLength == bytes &&
        !memcmp( current.info.Data, directory, bytes )) goto flush;
    if (status && status != STATUS_OBJECT_NAME_NOT_FOUND && status != STATUS_BUFFER_TOO_SMALL &&
        status != STATUS_BUFFER_OVERFLOW) goto done;
    status = NtSetValueKey( key, &value, 0, REG_SZ, directory, bytes );
    if (status) goto done;
    *changed = 1;
flush:
    status = NtFlushKey( key ); /* Also retry a previous failed flush of the same value. */
done:
    NtClose( key );
    return status;
}
