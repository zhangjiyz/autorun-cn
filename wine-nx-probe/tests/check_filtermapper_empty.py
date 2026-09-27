#!/usr/bin/env python3
"""Exercise Quartz's actual empty-category guard and enumerator lifecycle.

The matching loop itself is outside this fixture. COM device enumeration is
mocked, while the function prefix and empty enumerator methods are extracted
unchanged from filtermapper.c.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "dlls/quartz/filtermapper.c").read_text()


def function(signature):
    start = source.index(signature)
    brace = source.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


start = source.index("static HRESULT WINAPI FilterMapper3_EnumMatchingFilters(")
end = source.index("    while (IEnumMoniker_Next(pEnumCat", start)
prefix = source[start:end]
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int32_t HRESULT, LONG;
typedef uint32_t ULONG, DWORD;
typedef int BOOL, GUID, CLSID, REGPINMEDIUM, IFilterMapper3;
typedef void *LPVOID;
#define WINAPI
#define TRACE(...) ((void)0)
#define FIXME(...) ((void)0)
#define FAILED(hr) ((hr) < 0)
#define S_OK 0
#define S_FALSE 1
#define E_OUTOFMEMORY ((HRESULT)0x8007000e)
#define E_FAIL ((HRESULT)0x80004005)
#define CLSCTX_INPROC 1
static GUID CLSID_SystemDeviceEnum, IID_ICreateDevEnum, CLSID_ActiveMovieCategories;
typedef struct { int refs; } IMoniker;
typedef struct { const void *lpVtbl; } IEnumMoniker;
typedef struct { int refs; } ICreateDevEnum;
struct enum_moniker {
    IEnumMoniker IEnumMoniker_iface;
    LONG refcount;
    unsigned int index, count;
    IMoniker **filters;
};
struct Vector { void *pData; unsigned int current, size; };
static const int enum_moniker_vtbl;
static struct enum_moniker *impl_from_IEnumMoniker(IEnumMoniker *iface)
{ return (struct enum_moniker *)iface; }
#define InterlockedDecrement(p) (--*(p))
#define IMoniker_AddRef(p) (++(p)->refs)
#define IMoniker_Release(p) (--(p)->refs)
static HRESULT create_result, category_result;
static ICreateDevEnum factory;
static unsigned int factory_releases, matching_loop_entries;
static HRESULT CoCreateInstance(const GUID *clsid, void *outer, int context,
                               const GUID *iid, LPVOID *out)
{
    (void)clsid; (void)outer; (void)context; (void)iid;
    *out = NULL;
    if (FAILED(create_result)) return create_result;
    factory.refs = 1;
    *out = &factory;
    return S_OK;
}
static HRESULT ICreateDevEnum_CreateClassEnumerator(ICreateDevEnum *iface,
                                        const CLSID *category, IEnumMoniker **out, DWORD flags)
{
    assert(iface == &factory && category == &CLSID_ActiveMovieCategories && !flags);
    *out = NULL;
    if (category_result == S_OK) *out = (IEnumMoniker *)0x1234;
    return category_result;
}
static ULONG ICreateDevEnum_Release(ICreateDevEnum *iface)
{ assert(iface == &factory && iface->refs == 1); factory_releases++; return --iface->refs; }
'''
suffix = r'''
    /* Normal matching-loop entry is only valid with a category enumerator. */
    assert(pEnumCat);
    (void)pMonikerCat; (void)monikers;
    matching_loop_entries++;
    ICreateDevEnum_Release(pCreateDevEnum);
    return 99;
}
static HRESULT match(IEnumMoniker **out)
{
    return FilterMapper3_EnumMatchingFilters(NULL, out, 0, 0, 0,
            0, 0, NULL, NULL, NULL, 0, 0, 0, NULL, NULL, NULL);
}
int main(void)
{
    IEnumMoniker *out = (void *)0xdeadbeef;
    IMoniker *moniker = NULL;
    ULONG fetched = 99;
    IMoniker registered = {2}, *filters[] = {&registered};
    assert(enum_moniker_create(filters, 1, &out) == S_OK);
    assert(enum_moniker_Next(out, 1, &moniker, &fetched) == S_OK);
    assert(moniker == &registered && fetched == 1 && registered.refs == 3);
    IMoniker_Release(moniker);
    assert(!enum_moniker_Release(out) && registered.refs == 1);
    moniker = NULL;
    fetched = 99;
    category_result = S_FALSE;
    assert(match(&out) == S_OK && out);
    assert(factory_releases == 1 && !factory.refs && !matching_loop_entries);
    assert(enum_moniker_Next(out, 1, &moniker, &fetched) == S_FALSE && !fetched);
    assert(!moniker && !enum_moniker_Release(out));
    category_result = E_FAIL;
    assert(match(&out) == E_FAIL && !out && factory_releases == 2);
    create_result = E_FAIL;
    assert(match(&out) == E_FAIL && !out && factory_releases == 2);
    create_result = S_OK;
    category_result = S_OK;
    assert(match(&out) == 99 && matching_loop_entries == 1 && factory_releases == 3);
    puts("Quartz empty category: valid empty enumerator, Next/Release, errors and normal dispatch passed");
}
'''
methods = "\n".join(function(signature) for signature in (
    "static ULONG WINAPI enum_moniker_Release(",
    "static HRESULT WINAPI enum_moniker_Next(",
    "static HRESULT enum_moniker_create(",
))
with tempfile.TemporaryDirectory(prefix="wine-nx-filtermapper-") as temporary:
    path = Path(temporary)
    (path / "test.c").write_text(fixture + methods + prefix + suffix)
    sanitizer = "undefined" if sys.platform == "darwin" else "address,undefined"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-parameter", "-fsanitize=" + sanitizer,
                    str(path / "test.c"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True, timeout=30)
