#!/usr/bin/env python3
"""Exercise production execution-mode selection with host stubs."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
dynarec = (root / 'wine-nx-probe/source/wow64_box64_dynarec.c').read_text()


def function(source, signature):
    start = source.index(signature + '\n{')
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


values_start = dynarec.index('static int *const nx_box64_values[')
values = dynarec[values_start:dynarec.index('\n};', values_start) + 3]
init_env = function(dynarec, 'static void init_box64_env(void)')
fields = sorted(set(re.findall(r'box64env\.(\w+)', values + init_env)))
mode_fixture = r'''
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wine-nx-probe/source/box64_options.h"
struct { FIELDS } box64env;
struct context { pthread_mutex_t mutex_dyndump, mutex_trace, mutex_tls, mutex_thread, mutex_bridge; };
static struct context context;
static struct context *my_context = &context;
static pthread_mutex_t arena_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static int dynarec_ready, arena_calls, jump_calls, arena_ok = 1;
static int nx_box64_selfmod;
char wine_nx_box64_options_path[512];
#define LOG_NONE 0
/* Other CPU/translator defaults do not participate in execution-mode selection. */
#define ENVSUPER()
static void init_jump_tables(void) { jump_calls++; }
static int create_arena(int unused) { arena_calls++; return arena_ok; }
void wine_nx_runtime_trace(const char *message) __attribute__((weak));
'''.replace('FIELDS', ' '.join('int ' + field + ';' for field in fields))
mode_fixture += values + '\n' + function(dynarec, 'static void apply_box64_options(void)')
mode_fixture += '\n' + init_env + '\n' + function(dynarec, 'static void init_dynarec(void)')
mode_fixture += '\n' + function(dynarec, 'int wine_nx_box64_dynarec_init(void)')
mode_fixture += r'''
int main(int argc, char **argv)
{
    int enabled = atoi(argv[2]);
    arena_ok = atoi(argv[3]);
    snprintf(wine_nx_box64_options_path, sizeof(wine_nx_box64_options_path), "%s", argv[1]);
    assert(wine_nx_box64_dynarec_init() == (enabled && arena_ok));
    assert(wine_nx_box64_dynarec_init() == (enabled && arena_ok));
    assert(arena_calls == enabled && jump_calls == 1);
    assert(box64env.dynarec == (enabled && arena_ok));
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='wine-nx-box64-mode-') as temporary:
    tmp = Path(temporary)

    def compile_(name, fixture, defines=(), extra=()):
        source = tmp / (name + '.c')
        executable = tmp / name
        source.write_text(fixture)
        subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-parameter', '-Wno-unused-function',
                        '-fsanitize=undefined', '-pthread', '-I', str(root),
                        *defines, str(source), *map(str, extra), '-o', str(executable)], check=True)
        return executable

    trace = tmp / 'trace.c'
    trace.write_text('void wine_nx_runtime_trace(const char *message) { (void)message; }\n')
    mode = compile_('mode', mode_fixture, extra=[trace])
    for name, text, enabled in [('missing', None, 1), ('interpreter', 'BOX64_DYNAREC=0\n', 0),
                                ('dynarec', 'BOX64_DYNAREC=1\n', 1),
                                ('invalid', 'BOX64_DYNAREC=2\n', 1),
                                ('unknown', 'UNKNOWN_FLAG=0\n', 1)]:
        settings = tmp / (name + '.box64.txt')
        if text is not None:
            settings.write_text(text)
        for arena_ok in (0, 1):
            subprocess.run([str(mode), str(settings), str(enabled), str(arena_ok)],
                           check=True, stdout=subprocess.PIPE, text=True, timeout=30)
    print('Execution mode: per-game override, default, invalid/unknown options, arena fallback and once-only initialization')
