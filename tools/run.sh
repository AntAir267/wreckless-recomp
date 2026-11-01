#!/usr/bin/env bash
# tools/run.sh NAME SECONDS [BIN] -- one headless bring-up run; log and frame
# dumps in work/runs/NAME. The title's runtime settings are main.c's
# defaults; extra RECOMP_* settings come from the caller's environment.
set -u
cd "$(dirname "$0")/.."
name=$1; secs=${2:-60}; bin=${3:-bin/wreckless}
out=work/runs/$name; rm -rf "$out"; mkdir -p "$out"
export RECOMP_WATCHDOG_SECS=${RECOMP_WATCHDOG_SECS:-$((secs + 60))}
export RECOMP_STUB_LOG=1 RECOMP_KERNEL_LOG_BUDGET=${RECOMP_KERNEL_LOG_BUDGET:-60}
export RECOMP_FB_DUMP=$out/f RECOMP_FB_DUMP_FLIPS=${EVERY:-60}
timeout -s TERM "$secs" "$bin" --headless --save-dir work/save > "$out/run.log" 2>&1
echo "exit=$? lines=$(wc -l < "$out/run.log") dumps=$(ls "$out" | grep -c bmp)"
