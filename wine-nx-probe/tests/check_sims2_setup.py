#!/usr/bin/env python3
"""The Sims 2 setup (wine-nx-probe/tools/sims2_setup.c), which writes what the
release's "Instalar Registros" batch file writes -- from inside the card, where
the paths are the card's.

The game reads EPsInstalled by position, so the list and the table of packs
have to say the same thing in the same order; an empty place in the list is a
place, and losing it moves every pack after it."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
source = (root / 'wine-nx-probe/tools/sims2_setup.c').read_text()

table = re.findall(r'\{\s*L"(Sims2[A-Z0-9]*\.exe)",\s*L"(\w+)",\s*L"([^"]+)"\s*\}', source)
assert len(table) == 17, len(table)
assert table[0] == ('Sims2.exe', 'Base', 'The Sims 2')
table = [(exe, folder) for exe, folder, _ in table]

# Releases rebuilt from the retail discs read HKLM\SOFTWARE\EA GAMES\<name>\
# Install Dir instead, which is what "The Sims 2 is not installed on this
# system" reads; the repack's own instructions name that key. Both are written.
ea = re.findall(r'\{\s*L"Sims2[A-Z0-9]*\.exe",\s*L"\w+",\s*L"([^"]+)"\s*\}', source)
assert len(ea) == len(set(ea)) == 17, ea
assert all(name.startswith('The Sims 2') for name in ea), ea
assert 'L"SOFTWARE\\\\EA GAMES\\\\"' in source
# Sims2EP9.exe contains these six value names and not Install Dir; it stops at
# the first one it cannot read, so every one of them has to be written.
for name in ('DisplayName', 'Locale', 'Language', 'Region', 'CacheSize', 'EPsInstalled'):
    assert f'L"{name}"' in source, name
assert 'set_machine_string( path, L"Install Dir", folder )' in source
assert 'set_machine_dword( path, L"CacheSize", 0x40000000 )' in source
# The retail list is built from what was found, not the constant: naming a pack
# whose key was never written is what "some required files have been deleted" is.
assert 'if (!i) ok &= set_machine_string( path, L"EPsInstalled", installed );' in source
assert 'installed_list( installed, INSTALLED_MAX, pack_found )' in source
assert 'if (found[i]) wide_append( out, &at, max, packs[i].exe );' in source
# and the version key the release's instructions name for a language change
assert 'wide_append( path, &at, MAX_PATH * 2, L"\\\\1.0" );' in source
assert 'set_machine_dword( path, L"Language", 0x13 )' in source
# The retail executable finds every pack through Windows' App Paths entry for it,
# opened by name before anything else -- the runtime's [REG] trace shows it
# opening App Paths\\Sims2.exe and App Paths\\Sims2EP9.exe, then reading
# EPsInstalled from the base game's entry. None of that existed, so the read
# fell on the root and the game said it was not installed.
assert 'L"SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\"' in source
assert 'set_machine_string( path, L"", executable )' in source           # the default: the exe
assert 'set_machine_string( path, L"Path", folder )' in source
assert 'set_machine_string( path, L"Game Registry", registry )' in source
assert 'if (!i) ok &= set_machine_string( path, L"EPsInstalled", installed );' in source

declaration = source[source.index('eps_installed[]'):]
declaration = declaration[:declaration.index(';')]
listed = ''.join(re.findall(r'L"([^"]*)"', declaration)).split(',')
assert len(listed) == 17, listed          # sixteen packs and the empty place
assert listed.count('') == 1, listed
assert listed[11] == '', listed           # where the release's own list has it

# The list is the table without the base game, in the same order.
assert [name for name in listed if name] == [exe for exe, _ in table[1:]]
# No folder named twice, or two packs would share one.
folders = [folder for _, folder in table]
assert len(folders) == len(set(folders)) == 17

# The base game and the newest expansion are the two that carry Game Registry.
assert 'if (!i || i + 1 == PACK_COUNT)' in source
# A pack that is not there gets no key, rather than one pointing at nothing.
assert 'pack_found[i] = find_pack( game, packs[i].exe, packs[i].folder, pack_path[i], MAX_PATH );' in source
assert 'if (!pack_found[i])' in source
assert 'continue;' in source
# A pack is matched by the executable in its TSBin, so a release that names the
# folders differently still resolves; check_sims2_layout.py runs that.
assert 'L"TSBin\\\\"' in source and 'file_exists( path )' in source

# The release's own anadius.cfg sets its language to "invalid" so the game reads
# the locale from a key of its own, and every number the setup takes has to have
# one: without it the game says "open: Invalid handle" and stops.
locales = re.findall(r'\{\s*(\d+), L"([a-z]{2}_[A-Z]{2})"\s*\}', source)
assert len(locales) == 22, len(locales)
numbers = [int(n) for n, _ in locales]
assert numbers == sorted(numbers) and len(set(numbers)) == 22
assert 12 not in numbers and 19 not in numbers      # the two the release skips
assert dict((int(n), l) for n, l in locales)[23] == 'pt_PT'
assert 'Software\\\\Maxis\\\\The Sims 2 Legacy' in source and 'L"Locale"' in source

# The game sits beside the setup, inside it, at the root of the drive or one
# folder further down, and more than one of those can answer: a card that held
# an older install has a folder with a pack or two still in it. Every candidate
# is weighed and the one with the most packs wins -- taking the first that
# answered gave the leftover, and the game said it was not installed.
assert source.count('consider( ') >= 4 and 'consider_below( above, game, &best, MAX_PATH )' in source
assert 'if (count <= *best_count) return;' in source
assert 'join( named, MAX_PATH, above, L"The Sims 2" )' in source
assert 'if (!best)' in source        # nothing anywhere is the only failure
# A path is joined without doubling the slash, which C:\ would.
assert "if (at && out[at - 1] != '\\\\') wide_append( out, &at, max, L\"\\\\\" );" in source
# And the trailing slash is kept only where it is part of the name.
assert source.count('out[n > 3 ? n - 1 : n] = 0;') == 2

print(f'sims2 setup: {len(folders)} packs, the list and the table in step, the empty place kept')
