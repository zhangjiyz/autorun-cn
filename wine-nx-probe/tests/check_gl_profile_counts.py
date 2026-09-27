#!/usr/bin/env python3
"""Exercise the actual GL report's interval deltas and bounded output."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "dlls/ntdll/unix/virtual.c").read_text()
start = source.index("void wine_nx_gl_profile(")
brace = source.index("{", start)
end, depth = brace + 1, 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1

fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
struct wine_nx_gl_profile_entry { unsigned long long time; unsigned int calls; };
static struct wine_nx_gl_profile_entry wine_nx_gl_profile_entries[8];
'''
tests = r'''
int main(void)
{
    char output[512], tiny[4] = {'x', 'x', 'x', 'x'};
    wine_nx_gl_profile(output, sizeof(output));
    assert(!*output);
    wine_nx_gl_profile_entries[1].time = 100000;
    wine_nx_gl_profile_entries[1].calls = 1;
    wine_nx_gl_profile_entries[2].time = 20000;
    wine_nx_gl_profile_entries[2].calls = 900;
    wine_nx_gl_profile_entries[3].calls = 800; /* Fast calls can take less than one timer tick. */
    wine_nx_gl_profile(output, sizeof(output));
    assert(!strcmp(output, " gl_top=1:10/1,2:2/900 gl_hot=2:900,3:800,1:1"));
    wine_nx_gl_profile_entries[2].time += 30000;
    wine_nx_gl_profile_entries[2].calls += 10;
    wine_nx_gl_profile(output, sizeof(output));
    assert(!strcmp(output, " gl_top=2:3/10 gl_hot=2:10"));
    wine_nx_gl_profile(output, sizeof(output));
    assert(!*output); /* A report consumes its own interval, not the lifetime totals. */
    wine_nx_gl_profile_entries[3].calls += 20;
    wine_nx_gl_profile(output, sizeof(output));
    assert(!strcmp(output, " gl_hot=3:20"));
    wine_nx_gl_profile_entries[3].calls++;
    wine_nx_gl_profile(tiny, 2);
    assert(tiny[1] == 0 && tiny[2] == 'x' && tiny[3] == 'x');
    wine_nx_gl_profile(NULL, 0);
    puts("GL profile: time/count ranking, zero-time calls, interval deltas and tiny/zero buffers passed");
}
'''
with tempfile.TemporaryDirectory(prefix="wine-nx-gl-counts-") as temporary:
    path = Path(temporary)
    (path / "test.c").write_text(fixture + source[start:end] + tests)
    sanitizer = "undefined" if sys.platform == "darwin" else "address,undefined"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=" + sanitizer,
                    str(path / "test.c"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True, timeout=30)
