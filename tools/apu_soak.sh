#!/usr/bin/env bash
# tools/apu_soak.sh [RUNS]: the stuck sound bug's check (NOTES.md, The release): the check, one
# mission with a trap's interrupt dropped on purpose, then RUNS more; for each, what the APU's
# counters did and any "[APU] front end stuck trapped" line.
cd "$(dirname "$0")/.."
tools/check.sh --configure | tail -2 | cut -c1-170
one() {
    local n=$1
    RECOMP_APU_TRACE=1 tools/mission_drive.sh "$n" 110 > "work/runs/$n.drive" 2>&1
    grep '\[APU-IRQ\]' "work/runs/$n/run.log" | tail -1 | sed "s/^/$n: /" | cut -c1-180
    echo "$n: stuck lines $(grep -c 'front end stuck trapped' "work/runs/$n/run.log")"
    grep -m2 'front end stuck trapped' "work/runs/$n/run.log" | cut -c1-130
}
RECOMP_DEBUG=apu_trap_lose=50 one soak_lose
grep -m1 'apu_trap_lose' work/runs/soak_lose/run.log | cut -c1-90
for i in $(seq "${1:-2}"); do one "soak_$i"; done
