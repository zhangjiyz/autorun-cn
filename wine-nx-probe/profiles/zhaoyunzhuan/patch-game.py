#!/usr/bin/env python3
"""Enable absolute mouse mode in the verified 2002ls Game.exe, with backup."""

import argparse
import hashlib
import json
import os
from pathlib import Path

definition = json.loads(Path(__file__).with_name("binary-patch.json").read_text())
if set(definition) != {"schema", "original_sha256", "patched_sha256", "offset", "old", "new"} or definition["schema"] != 1:
    raise RuntimeError("invalid binary-patch.json")
ORIGINAL_SHA256 = definition["original_sha256"]
PATCHED_SHA256 = definition["patched_sha256"]
OFFSET = definition["offset"]
OLD = bytes.fromhex(definition["old"])
NEW = bytes.fromhex(definition["new"])


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def write_verified(path: Path, data: bytes, expected: str) -> None:
    temp = path.with_name(path.name + ".autorun-patch.tmp")
    try:
        with temp.open("wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        if digest(temp.read_bytes()) != expected:
            raise RuntimeError("staged file failed verification")
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game", type=Path, help="path to your 2002ls Game.exe")
    parser.add_argument("--restore", action="store_true", help="restore the original Game.exe")
    args = parser.parse_args()
    game = args.game
    backup = game.with_name(game.name + ".autorun-before-absolute-mouse")
    data = game.read_bytes()
    current = digest(data)

    if args.restore:
        if current != PATCHED_SHA256 or not backup.is_file():
            parser.error("patched Game.exe or verified backup is missing")
        original = backup.read_bytes()
        if digest(original) != ORIGINAL_SHA256:
            parser.error("backup does not match the supported Game.exe")
        write_verified(game, original, ORIGINAL_SHA256)
        print(f"Restored original Game.exe from {backup}")
        return

    if current == PATCHED_SHA256:
        print("Game.exe is already patched")
        return
    if current != ORIGINAL_SHA256 or data[OFFSET:OFFSET + len(OLD)] != OLD:
        parser.error("unsupported Game.exe; no changes made")
    if backup.exists():
        if digest(backup.read_bytes()) != ORIGINAL_SHA256:
            parser.error("existing backup is not the expected original; no changes made")
    else:
        write_verified(backup, data, ORIGINAL_SHA256)
    patched = data[:OFFSET] + NEW + data[OFFSET + len(NEW):]
    if digest(patched) != PATCHED_SHA256:
        parser.error("patched data failed verification; no changes made")
    write_verified(game, patched, PATCHED_SHA256)
    print(f"Enabled absolute mouse mode. Original backed up at {backup}")


if __name__ == "__main__":
    main()
