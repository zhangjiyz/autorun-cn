#!/bin/sh
set -eu
root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
exec python3 "$root/wine-nx-probe/tools/local-ci.py" "$@"
