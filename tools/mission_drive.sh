#!/usr/bin/env bash
# tools/mission_drive.sh NAME [SECS]: headless, scripted into mission A-1 (Normal),
# the cutscene skipped, the throttle held into the park; frametime CSV in the run.
# BIN= another binary; DELAY= ms to wait at the title first (an attract demo).
cd "$(dirname "$0")/.."
name=$1; secs=${2:-150}; out=work/runs/$name
: > work/padlive
(RECOMP_PAD_LIVE=$PWD/work/padlive \
 RECOMP_PAD_SCRIPT="$(python3 -c "import sys; d=int(sys.argv[1]); print(','.join(f'{int(t)+d}:{b}' for t,b in (x.split(':',1) for x in '6000:start,9000:a,12000:a,15000:a,18000:a,23000:a,28000:left,30000:a,34000:a'.split(','))))" ${DELAY:-0})" \
 RECOMP_TRACE="frametime=$PWD/work/ft-$name.csv${TRACE:+,$TRACE}" EVERY=${EVERY:-120} \
 tools/run.sh $name $secs ${BIN:-bin/wreckless} > work/runs/$name.out 2>&1 &)
sleep 2
until grep -q 'mapA01\\st_demo.tac' $out/run.log 2>/dev/null; do sleep 2; done
sleep 4; echo start >> work/padlive
sleep 8; printf 'rt:3000\nrt+lleft:1500\nrt:30000\n' >> work/padlive
until grep -q '^exit=' work/runs/$name.out 2>/dev/null; do sleep 2; done
cat work/runs/$name.out
