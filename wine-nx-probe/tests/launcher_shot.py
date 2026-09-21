#!/usr/bin/env python3
"""The library fixture and what its screenshots must show, for check-launcher-host.sh.

A library larger than one screenful is staged here, each game with art of one
flat colour of its own, so a screenshot says which games the grid is showing and
whether every card kept its own cover. The launcher draws the grid at 1280x720
with launcher.c's grid_layout geometry, which puts the middle of each card at a
fixed point; the colour there is the game's, dimmed the way a card that does not
have the focus is drawn.

  launcher_shot.py stage <card> <exe> <count>      games, art and the catalog
  launcher_shot.py check <shot.png> <first> <slot> the fourteen cards it must show
"""
import struct
import sys
import zlib
from pathlib import Path

# launcher.c: SHELL_MARGIN 84, UI_HEADER_HEIGHT 80, FOOTER_SPACE 38, card 140x210,
# gaps 22 and 16, caption 36: seven columns over two rows at 1280x720.
X0, Y0, CARD, CARD_H, GAP_X, GAP_Y, CAPTION, COLUMNS, ROWS = 84, 131, 140, 210, 22, 16, 36, 7, 2
# draw_card dims what does not have the focus; the selected card is drawn whole.
DIM = 190


def colour_of(game):
    """Far enough apart that a neighbour's cover cannot pass for this one, and
    bright enough that a card still waiting for its art cannot either."""
    return (80 + (game * 37) % 160, 80 + (game * 91) % 160, 80 + (game * 53) % 160)


def has_art(colour):
    """A card with no art yet is the focus fill or the card's own dark grey."""
    return max(colour) >= 50


def write_png(path, width, height, rgb):
    def chunk(tag, data):
        body = tag + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body))

    raw = b''.join(b'\0' + bytes(rgb) * width for _ in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' +
                     chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
                     chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def stage(card, exe, count):
    lines = ['version=3']
    for game in range(count):
        folder = card / f'games/Game {game:03d}'
        folder.mkdir(parents=True, exist_ok=True)
        program = folder / f'game{game:03d}.exe'
        if not program.exists():
            program.symlink_to(exe)
        write_png(folder / 'square.png', 512, 512, colour_of(game))
        write_png(folder / 'portrait.png', 600, 900, colour_of(game))
        lines += ['', f'[game {game + 1}]',
                  f'path=sdmc:/games/Game {game:03d}/game{game:03d}.exe',
                  f'title=Game {game:03d}', f'added-order={game + 1}',
                  'launched-order=0', 'favorite=0',
                  f'square-art=sdmc:/games/Game {game:03d}/square.png',
                  f'portrait-art=sdmc:/games/Game {game:03d}/portrait.png']
    (card / 'switch/wine').mkdir(parents=True, exist_ok=True)
    (card / 'switch/wine/launcher-library-v2.ini').write_text('\n'.join(lines) + '\n')


def read_png(path):
    data = Path(path).read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', path
    pos, idat, width, height, channels = 8, b'', 0, 0, 0
    while pos < len(data):
        length, tag = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += length + 12
        if tag == b'IHDR':
            width, height, depth, colour = struct.unpack('>IIBB', body[:10])
            assert depth == 8 and colour in (2, 6), (depth, colour)
            channels = 3 if colour == 2 else 4
        elif tag == b'IDAT':
            idat += body
        elif tag == b'IEND':
            break
    raw, stride = zlib.decompress(idat), width * channels
    rows, previous, pos = [], bytearray(stride), 0
    for _ in range(height):
        filter_type = raw[pos]
        line = bytearray(raw[pos + 1:pos + 1 + stride])
        pos += stride + 1
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = previous[i]
            c = previous[i - channels] if i >= channels else 0
            if filter_type == 1:
                line[i] = (line[i] + a) & 0xff
            elif filter_type == 2:
                line[i] = (line[i] + b) & 0xff
            elif filter_type == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xff
            elif filter_type == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xff
        rows.append(bytes(line))
        previous = line
    return width, height, channels, rows


def card_colours(path):
    width, height, channels, rows = read_png(path)
    assert (width, height) == (1280, 720), (width, height)
    colours = []
    for row in range(ROWS):
        for column in range(COLUMNS):
            x = X0 + column * (CARD + GAP_X) + CARD // 2
            y = Y0 + row * (CARD_H + CAPTION + GAP_Y) + CARD_H // 2
            pixel = rows[y][x * channels:x * channels + 3]
            colours.append((pixel[0], pixel[1], pixel[2]))
    return colours


def check(path, first, slot, least=10):
    """Every card shows its own game's cover, at the brightness it is drawn with.

    A shot taken while the covers are still being decoded has cards with no art
    yet; `least` says how many must have arrived for the shot to say anything.
    """
    wrong, seen = [], 0
    for index, shown in enumerate(card_colours(path)):
        game = first + index
        full = colour_of(game)
        want = full if index == slot else tuple(v * DIM // 255 for v in full)
        if not has_art(shown):
            continue
        seen += 1
        # A rounding step either way; a cover from another game is far outside it.
        if max(abs(a - b) for a, b in zip(shown, want)) > 2:
            wrong.append(f'card {index} shows {shown}, not game {game} as {want}')
    if seen < least:
        wrong.append(f'only {seen} of the fourteen cards had a cover, wanted {least}')
    if wrong:
        print(f'{path}:', *wrong, sep='\n  ')
        return 1
    print(f'{Path(path).name}: {seen} cards, games {first} to {first + COLUMNS * ROWS - 1}, cover by cover')
    return 0


if __name__ == '__main__':
    if sys.argv[1] == 'stage':
        stage(Path(sys.argv[2]), Path(sys.argv[3]), int(sys.argv[4]))
    else:
        sys.exit(check(sys.argv[2], int(sys.argv[3]), int(sys.argv[4]),
                       int(sys.argv[5]) if len(sys.argv) > 5 else 10))
