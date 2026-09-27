#!/usr/bin/env python3
"""Exercise the actual message-box entry point with host-side UI stubs.

Invalid owners must fail before allocating or entering a modal dialog. Valid
owners and ownerless task-modal boxes must retain their normal dialog flow.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'dlls/user32/msgbox.c').read_text()
start = source.index('INT WINAPI MessageBoxIndirectW( LPMSGBOXPARAMSW msgbox )')
function = source[start:]
fixture = r'''
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#define WINE_NX_PE_LOADER
#include "windef.h"
#include "winbase.h"
#include "winuser.h"

struct ThreadWindows { UINT numHandles, numAllocs; HWND *handles; };
static HMODULE user32_module = (HMODULE)1;
static unsigned int resources, dialogs, allocations, frees, enumerations, enables, startup;
static DWORD last_error;
static HWND dialog_owner;
static BOOL resource_available = TRUE;

static BOOL valid_window(HWND hwnd) { return hwnd == (HWND)0x10028; }
static void last_error_set(DWORD error) { last_error = error; }
static HRSRC resource_find(HMODULE module, LPCWSTR type, LPCWSTR name, WORD language)
{
    resources++;
    return resource_available ? (HRSRC)1 : NULL;
}
static HGLOBAL resource_load(HMODULE module, HRSRC resource) { return (HGLOBAL)1; }
static HANDLE heap_get(void) { return (HANDLE)1; }
static LPVOID heap_alloc(HANDLE heap, DWORD flags, SIZE_T size)
{
    allocations++;
    return malloc(size);
}
static BOOL heap_free(HANDLE heap, DWORD flags, LPVOID memory)
{
    frees++;
    free(memory);
    return TRUE;
}
static BOOL CALLBACK enum_proc(HWND hwnd, LPARAM param) { return TRUE; }
static BOOL windows_enum(DWORD tid, WNDENUMPROC proc, LPARAM param)
{
    struct ThreadWindows *windows = (struct ThreadWindows *)param;
    enumerations++;
    windows->handles[windows->numHandles++] = (HWND)0x10028;
    return TRUE;
}
static BOOL window_enable(HWND hwnd, BOOL enabled)
{
    assert(hwnd == (HWND)0x10028 && enabled);
    enables++;
    return TRUE;
}
static INT_PTR CALLBACK dialog_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) { return 0; }
static INT_PTR dialog_run(HINSTANCE instance, LPCDLGTEMPLATEW template, HWND owner,
                          DLGPROC proc, LPARAM param)
{
    dialogs++;
    dialog_owner = owner;
    return IDCANCEL;
}
static void startup_modify(DWORD mask, DWORD flags) { startup++; }
#define IsWindow valid_window
#define SetLastError last_error_set
#define FindResourceExW resource_find
#define LoadResource resource_load
#define GetProcessHeap heap_get
#define HeapAlloc heap_alloc
#define HeapFree heap_free
#define MSGBOX_EnumProc enum_proc
#define GetCurrentThreadId() 1
#define EnumThreadWindows windows_enum
#define NtUserEnableWindow window_enable
#define MSGBOX_DlgProc dialog_proc
#define DialogBoxIndirectParamW dialog_run
#define NtUserModifyUserStartupInfoFlags startup_modify
#define WARN(...) ((void)0)
#define ERR(...) ((void)0)
''' + function + r'''
static void reset(void)
{
    resources = dialogs = allocations = frees = enumerations = enables = startup = 0;
    last_error = 0xdeadbeef;
    dialog_owner = (HWND)0xdeadbeef;
}
int main(void)
{
    MSGBOXPARAMSW params = {0};
    const HWND invalid[] = {(HWND)1, (HWND)0xdeadbeef};
    unsigned int i;
    params.cbSize = sizeof(params);
    params.dwStyle = MB_OKCANCEL | MB_ICONQUESTION;
    for (i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i)
    {
        reset();
        params.hwndOwner = invalid[i];
        assert(MessageBoxIndirectW(&params) == 0);
        assert(last_error == ERROR_INVALID_WINDOW_HANDLE);
        assert(!resources && !dialogs && !allocations && !enumerations && !startup);
    }
    reset();
    params.hwndOwner = (HWND)0x10028;
    assert(MessageBoxIndirectW(&params) == IDCANCEL);
    assert(resources == 1 && dialogs == 1 && startup == 1);
    assert(dialog_owner == params.hwndOwner && last_error == 0xdeadbeef);
    assert(!allocations && !enumerations);

    reset();
    params.hwndOwner = NULL;
    assert(MessageBoxIndirectW(&params) == IDCANCEL);
    assert(dialogs == 1 && dialog_owner == NULL && !allocations && !enumerations);

    reset();
    params.dwStyle |= MB_TASKMODAL;
    assert(MessageBoxIndirectW(&params) == IDCANCEL);
    assert(dialogs == 1 && allocations == 1 && frees == 1 && enumerations == 1 && enables == 1);

    reset();
    params.hwndOwner = (HWND)1;
    assert(MessageBoxIndirectW(&params) == 0 && last_error == ERROR_INVALID_WINDOW_HANDLE);
    assert(!resources && !dialogs && !allocations && !enumerations);

    reset();
    resource_available = FALSE;
    params.hwndOwner = NULL;
    assert(MessageBoxIndirectW(&params) == 0);
    assert(resources == 1 && !dialogs && !allocations && !enumerations);
    puts("MessageBox: invalid owners rejected; valid, ownerless and task-modal dialog flows preserved");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='wine-nx-messagebox-') as temporary:
    tmp = Path(temporary)
    c = tmp / 'test.c'
    c.write_text(fixture)
    executable = tmp / 'test'
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-fshort-wchar', '-fsanitize=undefined',
                    '-I', str(root / 'include'), str(c), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True, timeout=30)
