/* Copyright 2026 Wine-NX contributors. LGPL-2.1-or-later.
 * Game-local proxy for Swordman's Miles 5.0m. All other exports forward to the
 * original DLL. Preference 18 controls the timer's SuspendThread protection:
 * when Horizon refuses running-thread suspension, Miles skips every callback.
 * Use Miles' own preference API to keep its audio service running instead.
 */
#include <windows.h>

#define ORIGINAL_DLL "mss32_autorun_original.dll"
#define TIMER_SUSPEND_PREFERENCE 18

typedef int (WINAPI *startup_func)(void);
typedef int (WINAPI *preference_func)(int, int);

static HMODULE original(void)
{
    HMODULE module = GetModuleHandleA(ORIGINAL_DLL);
    return module ? module : LoadLibraryA(ORIGINAL_DLL);
}

int WINAPI proxy_startup(void)
{
    HMODULE module = original();
    startup_func startup = module ? (startup_func)GetProcAddress(module, "_AIL_startup@0") : NULL;
    preference_func set = module ? (preference_func)GetProcAddress(module, "_AIL_set_preference@8") : NULL;
    int result;

    if (!startup || !set)
    {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return 0;
    }
    result = startup();
    set(TIMER_SUSPEND_PREFERENCE, 0);
    return result;
}

int WINAPI proxy_set_preference(int preference, int value)
{
    HMODULE module = original();
    preference_func set = module ? (preference_func)GetProcAddress(module, "_AIL_set_preference@8") : NULL;

    if (!set)
    {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return -1;
    }
    return set(preference, preference == TIMER_SUSPEND_PREFERENCE ? 0 : value);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    (void)instance;
    (void)reason;
    (void)reserved;
    return TRUE;
}
