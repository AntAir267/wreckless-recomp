#!/usr/bin/env bash
# Regenerate the recompiled C from default.xbe with the xboxrecomp toolkit.
#
# The Double-S.T.E.A.L 2 recomp's regen.sh, itself
# the shape of sp00nznet/wreckless's, on the takedawn branch's toolkit only.
#
# Order matters: disasm rewrites functions.json from scratch, so any naming
# pass has to run after it, and recomp last.
#
#   xbe_parser    section layout -> build/xr/analysis.json
#   disasm        every code section, never --text-only: without the XDK
#                 sections (D3D, DSOUND, WMVDEC, XPP...) there are no function
#                 boundaries there and each function is glued to the next.
#   xdk names     the toolkit's XDK name list, by signature
#   func_id       library-function identification (CRT, XDK)
#   abi_analysis  calling convention / params / return type
#   recomp        emits the C into src/game/recomp/gen (gitignored)
#
# Usage: ./regen.sh [--disasm]     (--disasm forces the disassembly again)
#
# The toolkit is ../antbox: AntAir267/antbox, the shared Xbox
# toolkit, checked out at the commit in
# ANTBOX_PIN. XBOXRECOMP_DIR names another.
# Generated code only builds against the toolkit that generated it.
#
# default.xbe is the US game (byte for byte DSTEAL_US.xbe). WR_XBE names
# another of the disc's XBEs (DSTEAL_JP.xbe, the original Double-S.T.E.A.L);
# those are different builds, so they get their own build/xr-<name> and
# recomp/gen-<name>, and their own seeds (config/seed_functions_<name>.json).

set -uo pipefail

WR="$(cd "$(dirname "$0")" && pwd)"
RECOMP="${XBOXRECOMP_DIR:-$WR/../antbox}"
PY="$WR/.venv/bin/python3"
XBE_NAME="${WR_XBE:-default.xbe}"
XBE="$WR/game/Wreckless - The Yakuza Missions/$XBE_NAME"
if [[ "$XBE_NAME" == default.xbe ]]; then
    SUFFIX=; SEEDS_JSON=seed_functions.json
else
    SUFFIX="-$(basename "$XBE_NAME" .xbe)"; SEEDS_JSON="seed_functions$SUFFIX.json"
fi
OUT="$WR/build/xr$SUFFIX"
GEN="$WR/src/game/recomp/gen$SUFFIX"

if [[ ! -f "$XBE" ]]; then
    echo "missing $XBE -- extract it from your own disc image"
    exit 1
fi
mkdir -p "$OUT"

# Seeds: entry points nothing in the code calls, so the detector cannot find
# them. Add them one at a time and verify each: an address that is not a
# function start truncates the function containing it. Each entry says how
# it was found.
#
# config/seed_functions.json is the hand list. config/seeds_crosscheck.json
# is generated (tools/gen_seeds.py): starts upstream xboxrecomp v0.16, Ghidra
# or sp00nznet/wreckless's runs find and this toolkit does not, kept to clean
# starts (only a branch can arrive there).
SEEDS=()
for f in "$SEEDS_JSON" "seeds_crosscheck$SUFFIX.json"; do
    [[ -f "$WR/config/$f" ]] && SEEDS+=(--seed-functions "$WR/config/$f")
done

if [[ "${1:-}" == "--disasm" || ! -f "$OUT/disasm/functions.json" ]]; then
    echo "==> xbe_parser"
    (cd "$RECOMP" && "$PY" -m tools.xbe_parser "$XBE" \
        --json "$OUT/analysis.json" --quiet) || exit 1

    echo "==> disasm"
    (cd "$RECOMP" && "$PY" -m tools.disasm "$XBE" \
        --analysis-json "$OUT/analysis.json" "${SEEDS[@]}" \
        -o "$OUT/disasm" -v) > "$OUT/disasm.log" 2>&1 || { tail -20 "$OUT/disasm.log"; exit 1; }
    tr '\r' '\n' < "$OUT/disasm.log" | grep -E "Total functions|Reachable|Seeded" || true

    echo "==> xdk names"
    (cd "$RECOMP" && "$PY" tools/ghidra_naming/merge_names.py --apply \
        --names-json     tools/ghidra_naming/xdk_names.json \
        --functions-json "$OUT/disasm/functions.json") | tail -1
fi

echo "==> func_id"
(cd "$RECOMP" && "$PY" -m tools.func_id "$XBE" \
    --functions "$OUT/disasm/functions.json" \
    --strings   "$OUT/disasm/strings.json" \
    --xrefs     "$OUT/disasm/xrefs.json" \
    -o "$OUT/func_id") > "$OUT/func_id.log" 2>&1 || { tail -20 "$OUT/func_id.log"; exit 1; }
tail -1 "$OUT/func_id.log"

# ABI recovery. Without it every function is lifted as cdecl / 0 params /
# int-or-void, which is a guess the generated signatures then carry.
echo "==> abi_analysis"
(cd "$RECOMP" && "$PY" -m tools.abi_analysis "$XBE" \
    --functions  "$OUT/disasm/functions.json" \
    --identified "$OUT/func_id/identified_functions.json" \
    --output-dir "$OUT/abi") > "$OUT/abi.log" 2>&1 || { tail -20 "$OUT/abi.log"; exit 1; }
tail -1 "$OUT/abi.log"

# --split 250: 1000 functions per file does not build in 15 GB.
# Native replacements (config/manual_functions.json) are not emitted; the
# hooks file's sub_X are emitted as sub_X_gen.
MANUAL=()
[[ -f "$WR/config/manual_functions.json" ]] && MANUAL+=(--manual-functions "$WR/config/manual_functions.json")
# Guest busy-waits lowered to sleeping waits (tools.recomp --spin-waits):
# D3D's KickOff wait, NOTES.md "Overhead".
SPIN=()
[[ -f "$WR/config/spin_waits.json" ]] && SPIN=(--spin-waits "$WR/config/spin_waits.json")
echo "==> recomp"
(cd "$RECOMP" && "$PY" -m tools.recomp "$XBE" --all --split 250 \
    --disasm-dir  "$OUT/disasm" \
    --func-id-dir "$OUT/func_id" \
    --abi-dir     "$OUT/abi" \
    --gen-dir     "$GEN" \
    "${MANUAL[@]}" \
    "${SPIN[@]}" \
    --exclude-manual "$WR/src/game/recomp/recomp_hooks.c" \
    -o "$OUT/recomp") > "$OUT/recomp.log" 2>&1 || { tail -30 "$OUT/recomp.log"; exit 1; }
grep -E "unresolved|Complete|functions|failed" "$OUT/recomp.log" | tail -12

echo "==> done"
