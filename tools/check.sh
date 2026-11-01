#!/usr/bin/env bash
# tools/check.sh [--configure]: this game's check of a toolkit candidate
# (NOTES.md, Layout): build, then the title and the attract demo in 3D
# (run chk_demo) and mission A-1's scripted drive at 33.3 ms frames (run
# chk_mis, "moved by the title"), both with the APU's [APU-IRQ] line every
# 5 s. The summary goes to work/check.txt. --configure makes build/linux
# against the system's SDL2 and glslang when there is no build yet. On another
# machine, copy src/game/recomp/gen over first: it is gitignored, and a stale
# copy fails the link on duplicate sub_X symbols, or builds old code.
set -u
cd "$(dirname "$0")/.."
mkdir -p work/runs
if [ "${1:-}" = --configure ] && [ ! -f build/linux/build.ninja ]; then
    cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
          -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 \
          -DCMAKE_EXE_LINKER_FLAGS=--ld-path=/usr/lib/llvm-20/bin/ld.lld \
          > work/check-configure.log 2>&1 || { echo "configure failed (work/check-configure.log)"; exit 1; }
fi
ninja -C build/linux > work/check-build.log 2>&1 \
    || { echo "build failed (work/check-build.log)"; tail -20 work/check-build.log; exit 1; }
RECOMP_APU_TRACE=1 EVERY=120 tools/run.sh chk_demo 60 > work/runs/chk_demo.out 2>&1
RECOMP_APU_TRACE=1 tools/mission_drive.sh chk_mis 110 > work/runs/chk_mis.drive 2>&1
{
    echo "host $(hostname), toolkit $(git -C "$(grep -m1 '^XBOXRECOMP_DIR' build/linux/CMakeCache.txt | cut -d= -f2)" rev-parse --short HEAD 2>/dev/null || echo '?')"
    echo "chk_demo: $(cat work/runs/chk_demo.out)"
    echo "chk_mis: $(tail -1 work/runs/chk_mis.drive); moved by the title: $(grep -c 'moved by the title' work/runs/chk_mis/run.log)"
    echo "last [APU-IRQ] line of chk_mis: $(grep '\[APU-IRQ\]' work/runs/chk_mis/run.log | tail -1)"
    python3 - <<'PY'
import csv, statistics as st
r = [x for x in csv.DictReader(open('work/ft-chk_mis.csv')) if float(x['t_ms']) > 40000]
f = [float(x['frame_ms']) for x in r]
print(f'chk_mis frames after 40 s: {len(f)}, mean {st.mean(f):.2f} ms, p99 {sorted(f)[int(len(f) * .99) - 1]:.2f} ms')
PY
} > work/check.txt 2>&1
cat work/check.txt
