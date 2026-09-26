#!/usr/bin/env python3
"""Which folder each Sims 2 pack is in (wine-nx-probe/tools/sims2_setup.c).

The setup used to look for folders named Base and EP1-EP9, which is what one
release calls them. Another spells them out -- "The Sims 2 Nightlife" -- and a
collection copied whole keeps a folder of its own around the packs, so the
setup found nothing and the game said "The Sims 2 is not installed on this
system". A pack is identified by the executable in its TSBin instead, which is
the same in every release and is already the name written to the registry.

The finding functions are compiled here over a real directory tree, with the
Win32 calls they use served from POSIX, and asked about both layouts."""
from pathlib import Path
import subprocess
import tempfile
import os
import sys

root = Path(__file__).resolve().parents[2]
source = (root / 'wine-nx-probe/tools/sims2_setup.c').read_text()


def function(marker):
    """The named function, from its marker to its closing brace."""
    start = source.index(marker)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


PACK_TABLE = source[source.index('static const struct { const WCHAR *exe, *folder, *ea; } packs[]'):]
PACK_TABLE = PACK_TABLE[:PACK_TABLE.index('#define PACK_COUNT')] + '#define PACK_COUNT (sizeof(packs) / sizeof(packs[0]))\n'

fixture = r'''
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

typedef int BOOL;
typedef unsigned int DWORD;
typedef void *HANDLE;
typedef wchar_t WCHAR;
#define MAX_PATH 260
#define TRUE 1
#define FALSE 0
#define INVALID_FILE_ATTRIBUTES 0xffffffffu
#define FILE_ATTRIBUTE_DIRECTORY 0x10u
#define INVALID_HANDLE_VALUE ((HANDLE)-1)

typedef struct { DWORD dwFileAttributes; WCHAR cFileName[MAX_PATH]; } WIN32_FIND_DATAW;

/* The card's paths are Windows ones; here they are the host's, so a backslash
 * becomes a slash and the drive letter is the temporary directory. */
static char scratch[4096];
static const char *narrow( const WCHAR *w )
{
    unsigned int i = 0;
    while (w[i] && i + 1 < sizeof(scratch)) { scratch[i] = (w[i] == '\\') ? '/' : (char)w[i]; i++; }
    scratch[i] = 0;
    return scratch;
}

static DWORD GetFileAttributesW( const WCHAR *path )
{
    struct stat info;
    if (stat( narrow( path ), &info )) return INVALID_FILE_ATTRIBUTES;
    return S_ISDIR(info.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : 0;
}

static HANDLE FindFirstFileW( const WCHAR *pattern, WIN32_FIND_DATAW *found );
static BOOL FindNextFileW( HANDLE search, WIN32_FIND_DATAW *found );
static BOOL FindClose( HANDLE search );
'''

fixture += function('static void wide_append(')
fixture += function('static void join(')
fixture += function('static BOOL file_exists(')
fixture += function('static BOOL pack_here(')
fixture += function('static BOOL find_pack(')
fixture += PACK_TABLE
fixture += function('static unsigned int count_packs(')
fixture += function('static void consider(')
fixture += function('static void consider_below(')
fixture += function('static void installed_list(')

fixture += r'''
/* The pattern is always "<folder>\*"; the directory is everything before it. */
static HANDLE FindFirstFileW( const WCHAR *pattern, WIN32_FIND_DATAW *found )
{
    char path[4096];
    size_t len;
    DIR *dir;

    snprintf( path, sizeof(path), "%s", narrow( pattern ) );
    len = strlen( path );
    if (len >= 2 && path[len - 1] == '*') path[len - 2] = 0;
    if (!(dir = opendir( path ))) return INVALID_HANDLE_VALUE;
    if (FindNextFileW( dir, found )) return dir;
    closedir( dir );
    return INVALID_HANDLE_VALUE;
}

static BOOL FindNextFileW( HANDLE search, WIN32_FIND_DATAW *found )
{
    struct dirent *entry;
    unsigned int i;

    if (!(entry = readdir( (DIR *)search ))) return FALSE;
    for (i = 0; entry->d_name[i] && i + 1 < MAX_PATH; i++) found->cFileName[i] = entry->d_name[i];
    found->cFileName[i] = 0;
    found->dwFileAttributes = (entry->d_type == DT_DIR) ? FILE_ATTRIBUTE_DIRECTORY : 0;
    return TRUE;
}

static BOOL FindClose( HANDLE search ) { return !closedir( (DIR *)search ); }

static void widen( WCHAR *out, const char *text )
{
    unsigned int i = 0;
    while (text[i]) { out[i] = text[i]; i++; }
    out[i] = 0;
}

int main( int argc, char **argv )
{
    WCHAR root[MAX_PATH], found[MAX_PATH];
    char narrowed[4096];

    assert( argc == 4 );
    widen( root, argv[1] );

    /* argv[2] is the executable to look for, argv[3] the folder it is in, or
     * "" when nothing should be found. */
    if (!argv[3][0])
    {
        WCHAR exe[MAX_PATH];
        widen( exe, argv[2] );
        assert( !find_pack( root, exe, L"Base", found, MAX_PATH ) );
        printf( "not found, as expected\n" );
        return 0;
    }
    if (!strcmp( argv[2], "<list>" ))
    {
        /* argv[3] is which packs are there, one character each from index 1. */
        static BOOL found[PACK_COUNT];
        static WCHAR list[512];
        unsigned int i;

        for (i = 1; i < PACK_COUNT; i++) found[i] = argv[3][i - 1] == '1';
        installed_list( list, 512, found );
        printf( "%s\n", narrow( list ) );
        return 0;
    }
    if (!strcmp( argv[2], "<root>" ))
    {
        /* What start() does: weigh each candidate and keep the best. */
        unsigned int best = 0;

        found[0] = 0;
        consider( root, found, &best, MAX_PATH );
        consider_below( root, found, &best, MAX_PATH );
        assert( best );
        snprintf( narrowed, sizeof(narrowed), "%s", narrow( found ) );
        printf( "%s %u\n", narrowed, best );
        assert( !strcmp( narrowed, argv[3] ) );
        return 0;
    }
    {
        WCHAR exe[MAX_PATH], known[MAX_PATH];
        widen( exe, argv[2] );
        widen( known, "EP9" );
        assert( find_pack( root, exe, known, found, MAX_PATH ) );
        snprintf( narrowed, sizeof(narrowed), "%s", narrow( found ) );
        printf( "%s\n", narrowed );
        assert( !strcmp( narrowed, argv[3] ) );
    }
    return 0;
}
'''

PACKS = [
    ('Sims2.exe', 'Base', 'The Sims 2'),
    ('Sims2EP1.exe', 'EP1', 'The Sims 2 University'),
    ('Sims2EP2.exe', 'EP2', 'The Sims 2 Nightlife'),
    ('Sims2EP3.exe', 'EP3', 'The Sims 2 Open For Business'),
    ('Sims2SP1.exe', 'SP1', 'The Sims 2 Family Fun Stuff'),
    ('Sims2SP2.exe', 'SP2', 'The Sims 2 Glamour Life Stuff'),
    ('Sims2EP4.exe', 'EP4', 'The Sims 2 Pets'),
    ('Sims2EP5.exe', 'EP5', 'The Sims 2 Seasons'),
    ('Sims2SP4.exe', 'SP4', 'The Sims 2 Celebration! Stuff'),
    ('Sims2SP5.exe', 'SP5', 'The Sims 2 H&M Fashion Stuff'),
    ('Sims2EP6.exe', 'EP6', 'The Sims 2 Bon Voyage'),
    ('Sims2SP6.exe', 'SP6', 'The Sims 2 Teen Style Stuff'),
    ('Sims2EP7.exe', 'EP7', 'The Sims 2 FreeTime'),
    ('Sims2SP7.exe', 'SP7', 'The Sims 2 Kitchen & Bath Interior Design Stuff'),
    ('Sims2SP8.exe', 'SP8', 'The Sims 2 IKEA Home Stuff'),
    ('Sims2EP8.exe', 'EP8', 'The Sims 2 Apartment Life'),
    ('Sims2EP9.exe', 'EP9', 'The Sims 2 Mansion and Garden Stuff'),
]


def build_tree(base, naming):
    for exe, short, long_name in PACKS:
        folder = base / (short if naming == 'short' else long_name)
        (folder / 'TSBin').mkdir(parents=True, exist_ok=True)
        (folder / 'TSBin' / exe).write_bytes(b'MZ')
        # The updaters sit beside it in every release and must not be matched.
        (folder / 'TSBin' / 'TS2UPD.exe').write_bytes(b'MZ')
    return base


with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    src = tmp / 'layout.c'
    src.write_text(fixture)
    binary = tmp / 'layout'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-O1', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-function', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', str(src), '-o', str(binary)], check=True)

    def ask(root_dir, exe, expected, count=None, raw=False, packs=None):
        out = subprocess.run([str(binary), str(root_dir), exe, packs if raw else expected],
                             capture_output=True, text=True)
        if raw:
            assert out.returncode == 0, out.stdout + out.stderr
            return out.stdout.strip()
        assert out.returncode == 0, f'{exe} under {root_dir}: {out.stdout}{out.stderr}'
        printed = out.stdout.strip()
        if count is not None:
            assert printed.split(' ')[-1] == str(count), f'{printed}, wanted {count} packs'
            printed = printed.rsplit(' ', 1)[0]
        return printed

    failures = []
    # 1. the layout the setup was written for, packs directly under the root
    short = build_tree(tmp / 'short', 'short')
    for exe, shortname, _ in PACKS:
        got = ask(short, exe, str(short / shortname))
        if got != str(short / shortname): failures.append((exe, got))

    # 2. a release that spells the folders out, same place
    long_root = build_tree(tmp / 'long', 'long')
    for exe, _, long_name in PACKS:
        got = ask(long_root, exe, str(long_root / long_name))
        if got != str(long_root / long_name): failures.append((exe, got))

    # 3. the collection keeping a folder of its own around the packs
    nested = build_tree(tmp / 'nested' / 'The Sims 2 Ultimate Collection', 'long')
    ask(tmp / 'nested', '<root>', str(nested))

    # 4. a pack that is not installed is not matched by its neighbours
    (tmp / 'partial').mkdir()
    (tmp / 'partial' / 'EP9' / 'TSBin').mkdir(parents=True)
    (tmp / 'partial' / 'EP9' / 'TSBin' / 'Sims2EP9.exe').write_bytes(b'MZ')
    ask(tmp / 'partial', 'Sims2EP1.exe', '')
    got = ask(tmp / 'partial', '<root>', str(tmp / 'partial'), count=1)

    # 5. the list the base game is told it has. It is read by position, and a
    # pack named in it but missing a key is how "some required files have been
    # deleted" was reached: EPsInstalled advertised SP5 and SP8 while those two
    # folders were never found, so the game opened keys that were not there.
    all_there = ask(tmp, '<list>', None, raw=True, packs='1' * 16)
    assert all_there.split(',') == [
        'Sims2EP1.exe', 'Sims2EP2.exe', 'Sims2EP3.exe', 'Sims2SP1.exe', 'Sims2SP2.exe',
        'Sims2EP4.exe', 'Sims2EP5.exe', 'Sims2SP4.exe', 'Sims2SP5.exe', 'Sims2EP6.exe',
        'Sims2SP6.exe', '', 'Sims2EP7.exe', 'Sims2SP7.exe', 'Sims2SP8.exe',
        'Sims2EP8.exe', 'Sims2EP9.exe'], all_there
    # SP5 is the 9th and SP8 the 15th; both leave their place empty.
    without = '1' * 16
    without = without[:8] + '0' + without[9:13] + '0' + without[14:]
    partial = ask(tmp, '<list>', None, raw=True, packs=without).split(',')
    assert len(partial) == 17, partial                  # the length never changes
    assert partial[8] == '' and partial[14] == '', partial   # SP5 and SP8 leave their places
    assert partial[11] == '', partial                        # and the release's own empty place
    assert partial[7] == 'Sims2SP4.exe' and partial[9] == 'Sims2EP6.exe', partial
    assert partial[13] == 'Sims2SP7.exe' and partial[15] == 'Sims2EP8.exe', partial
    assert 'Sims2SP5.exe' not in partial and 'Sims2SP8.exe' not in partial, partial

    # 6. what the card actually had: one pack left behind by an older install,
    # in the very folder the readme names, against the real collection deeper
    # down. Taking the first root that answered gave the leftover and one pack.
    leftover = tmp / 'mixed'
    (leftover / 'The Sims 2' / 'EP9' / 'TSBin').mkdir(parents=True)
    (leftover / 'The Sims 2' / 'EP9' / 'TSBin' / 'Sims2EP9.exe').write_bytes(b'MZ')
    build_tree(leftover / 'The Sims 2 Ultimate Collection', 'long')
    ask(leftover, '<root>', str(leftover / 'The Sims 2 Ultimate Collection'), count=17)

if failures:
    for exe, got in failures:
        print('FAIL:', exe, '->', got)
    sys.exit(1)
print(f'sims2 layout: {len(PACKS)} packs found by their executable in short and '
      'spelled-out folder names, through a collection folder, a missing pack stays '
      'missing, the collection beats a leftover install, and EPsInstalled names '
      'only the packs that got a key')
