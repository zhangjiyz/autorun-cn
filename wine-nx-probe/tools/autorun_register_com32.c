/* Per-game 32-bit COM registration. The runtime supplies a validated basename
 * in C:\windows\autorun-com32-request.txt and checks this process's exit code. */
#include <windows.h>
#include <winternl.h>

__declspec(dllimport) NTSTATUS NTAPI NtDisplayString( const UNICODE_STRING *str );

static void report( const char *message, DWORD value )
{
    static const char hex[] = "0123456789abcdef";
    static const char prefix[] = "[AUTORUN COM32] ";
    WCHAR buffer[200];
    UNICODE_STRING line;
    unsigned int i, n = 0;
    for (i = 0; prefix[i]; i++) buffer[n++] = prefix[i];
    for (i = 0; message[i] && n < 170; i++) buffer[n++] = message[i];
    buffer[n++] = ' ';
    buffer[n++] = '0'; buffer[n++] = 'x';
    for (i = 0; i < 8; i++) buffer[n++] = hex[(value >> (28 - i * 4)) & 15];
    line.Buffer = buffer;
    line.Length = n * sizeof(WCHAR);
    line.MaximumLength = line.Length;
    NtDisplayString( &line );
}

static int valid_name( const char *name )
{
    unsigned int i = 0, j;
    while (name[i] && i < 48)
    {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) break;
        i++;
    }
    if (i < 1 || i > 44 || name[i] != '.' || name[i + 1] != 'd' ||
        name[i + 2] != 'l' || name[i + 3] != 'l' || name[i + 4]) return 0;
    for (j = 0; j < i; j++) if (name[j] == '.') return 0;
    return 1;
}

void __stdcall start(void)
{
    static const WCHAR base[] = L"C:\\windows\\syswow64\\";
    char request[64], name[49];
    WCHAR path[80];
    DWORD length = 0;
    unsigned int i, j;
    HANDLE file;
    HMODULE module;
    HRESULT (WINAPI *register_server)(void);
    HRESULT hr;

    file = CreateFileA( "C:\\windows\\autorun-com32-request.txt", GENERIC_READ, FILE_SHARE_READ,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if (file == INVALID_HANDLE_VALUE) { report( "request missing", GetLastError() ); ExitProcess( 1 ); }
    if (!ReadFile( file, request, sizeof(request), &length, NULL ))
    {
        DWORD error = GetLastError();
        CloseHandle( file ); report( "request read failed", error ); ExitProcess( 2 );
    }
    CloseHandle( file );
    if (!length || length >= sizeof(request)) { report( "invalid request length", length ); ExitProcess( 3 ); }
    for (i = 0; i < length && request[i] != '\n' && request[i] != '\r'; i++)
        if (i >= sizeof(name) - 1) { report( "invalid request name", i ); ExitProcess( 3 ); }
    for (j = 0; j < i; j++) name[j] = request[j];
    name[i] = 0;
    if (!valid_name( name ) || (i < length && request[i] == '\r' && ++i < length && request[i] != '\n') ||
        (i < length && ++i != length))
    {
        report( "invalid request name", length ); ExitProcess( 3 );
    }
    for (i = 0; base[i]; i++) path[i] = base[i];
    for (j = 0; name[j]; j++) path[i++] = (unsigned char)name[j];
    path[i] = 0;
    module = LoadLibraryExW( path, NULL, 0 );
    if (!module) { report( "DLL load failed", GetLastError() ); ExitProcess( 4 ); }
    register_server = (void *)GetProcAddress( module, "DllRegisterServer" );
    if (!register_server) { report( "DllRegisterServer missing", GetLastError() ); ExitProcess( 5 ); }
    hr = register_server();
    report( "DllRegisterServer result", (DWORD)hr );
    FreeLibrary( module );
    ExitProcess( SUCCEEDED(hr) ? 0 : 6 );
}
