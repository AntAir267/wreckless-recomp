#!/usr/bin/env bash
# package.sh [DEST] - the built game as a folder to run from.
#
# DEST (default dist/wreckless) gets the binary, the default settings
# (enhance.toml, only when DEST has none: a player's own is kept), the
# README, the licenses and a VERSION file naming both trees' commits. The
# game's files are not part of it: run it with --game-dir pointing at your
# extracted disc, and --save-dir where saves and the cache should go
# (~/.local/share/wreckless by default).
#
# WR_BIN names another build's binary (deck-build.sh's build/deck/bin for
# SteamOS). It needs the libraries the build links (SDL2, libepoxy,
# OpenSSL; Vulkan at run time), all of them ordinary system packages.
set -euo pipefail

WR="$(cd "$(dirname "$0")" && pwd)"
RT="${XBOXRECOMP_DIR:-$WR/../antbox}"
DEST="${1:-$WR/dist/wreckless}"

BIN="${WR_BIN:-$WR/bin/wreckless}"
[[ "$BIN" = /* ]] || BIN="$WR/$BIN"
if [[ ! -x "$BIN" ]]; then
    echo "no $BIN: build first (README.md, Building)" >&2
    exit 1
fi

mkdir -p "$DEST"
cp "$BIN" "$DEST/wreckless.new"
mv -f "$DEST/wreckless.new" "$DEST/wreckless"   # a running copy keeps its file
[[ -f "$DEST/enhance.toml" ]] || cp "$WR/config/enhance.toml" "$DEST/enhance.toml"
cp "$WR/README.md" "$WR/LICENSE" "$WR/LICENSE.wreckless" "$DEST/"
{
    echo "wreckless-recomp $(git -C "$WR" rev-parse --short HEAD)$(git -C "$WR" diff --quiet || echo '+changes')"
    echo "runtime antbox $(git -C "$RT" rev-parse --short HEAD)$(git -C "$RT" diff --quiet || echo '+changes')"
    echo "built $(date -u -r "$BIN" +%Y-%m-%dT%H:%MZ) ($(basename "$(dirname "$(dirname "$BIN")")")), needs $(objdump -T "$BIN" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1)"
} > "$DEST/VERSION"
echo "packaged in $DEST:"
cat "$DEST/VERSION"
