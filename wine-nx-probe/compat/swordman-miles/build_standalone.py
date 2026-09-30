#!/usr/bin/env python3
"""Build the complete game-local Miles DLL with the Swordman timer fix.

The input is only needed by the maintainer when building the adaptation package.
Installing the resulting DLL requires no original DLL or filename alias.
"""
import argparse
import hashlib
from pathlib import Path

from build import MILES_SHA256, exports, pe_image

PATCH_OFFSET = 0x13bd
PATCHED_SHA256 = 'f2d040218d7e63f83c799b005f688c1b4b150994442996b4c4929a46a1f15743'


def build(original, output):
    if original.resolve() == output.resolve():
        raise ValueError('Output must not overwrite the input DLL')
    raw, image, _ = pe_image(original)
    if hashlib.sha256(raw).hexdigest() != MILES_SHA256:
        raise ValueError('Unsupported Miles 5.0m input DLL')
    # Keep the preference load and its relocations intact. Zero its local
    # snapshot instead of testing it: the existing conditional skips SuspendThread,
    # and the matching ResumeThread check reads the same zero snapshot.
    anchor = bytes.fromhex('a14ca3042185c0a3b4a304217415')
    if raw[PATCH_OFFSET - 5:PATCH_OFFSET + 9] != anchor or image[PATCH_OFFSET - 5:PATCH_OFFSET + 9] != anchor:
        raise ValueError('Unexpected Miles timer layout')
    fixed = bytearray(raw)
    fixed[PATCH_OFFSET:PATCH_OFFSET + 2] = bytes.fromhex('31c0')
    if hashlib.sha256(fixed).hexdigest() != PATCHED_SHA256:
        raise ValueError('Unexpected patched DLL digest')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(fixed)
    before, after = exports(original), exports(output)
    assert len(after) == 315 and before == after
    assert all(forwarder is None for _, _, forwarder in after)
    assert [i for i, (old, new) in enumerate(zip(raw, fixed)) if old != new] == [PATCH_OFFSET]
    return fixed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        fixed = build(args.original, args.output)
    except (ValueError, OSError) as error:
        parser.exit(1, f'Miles standalone DLL: {error}\n')
    print(f'{args.output}: {len(fixed)} bytes, SHA256 {hashlib.sha256(fixed).hexdigest()}')
    print('315 original exports retained; one opcode changed; no proxy/original alias dependency')


if __name__ == '__main__':
    main()
