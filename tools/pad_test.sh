#!/usr/bin/env bash
# tools/pad_test.sh: the rule that any controller drives player 1, in a real
# window with virtual controllers (tools/vpad.py), no pad script. Pad 1 is in
# before the game starts, pad 2 comes late and presses first; each moves the
# main menu's cursor; pad 2 is unplugged and pad 1 goes on; a late pad 3's first
# press moves it; pad 4 rests off-centre and must hold nothing down. With a
# passing toolkit the cursor goes 0, 1, 2, 1, 2, 3, 2, stays, 3 (shots, in order).
# Run with SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1 unless the window has the focus.
# Env: BIN, OUT. Shots, [INPUT] lines: work/runs/$OUT.
cd "$(dirname "$0")/.."
out=work/runs/${OUT:-pad_test}; rm -rf "$out" work/save-pad; mkdir -p "$out"; cp -a work/save work/save-pad
: > "$out/ovl"
for n in 1 2 3 4; do mkfifo "$out/p$n"; done
pad() { python3 tools/vpad.py "VPad$1" < "$out/p$1" > /dev/null & eval "exec $((10+$1))>$out/p$1"; }
send() { echo "$2" >&$((10+$1)); echo "$(date +%T) pad$1 $2" >> "$out/steps"; }
unplug() { send "$1" quit; eval "exec $((10+$1))>&-"; }
shot() { sleep 0.9; echo "shot $PWD/$out/$1.bmp" >> "$out/ovl"; sleep 0.4; }
pad 1; sleep 1
RECOMP_OVERLAY_TEST=$PWD/$out/ovl timeout -s TERM 70 "${BIN:-bin/wreckless}" --save-dir work/save-pad > "$out/run.log" 2>&1 &
g=$!
sleep 5; pad 2; sleep 2.5; shot 00_title
send 2 start; sleep 2.2; shot 01_first_tap
send 2 start; sleep 2.5; shot 02_main_menu_cursor0
send 2 down; shot 03_pad2_down_cursor1
send 1 down; shot 04_pad1_down_cursor2
send 2 up; shot 05_pad2_up_cursor1
unplug 2; sleep 1; send 1 down; shot 06_pad1_alone_cursor2
pad 3; sleep 1.2; send 3 down; shot 07_late_pad3_cursor3
send 1 up; shot 08_pad1_up_cursor2
REST=ly=25000,lt=255,rx=-20000 pad 4; sleep 2.5; shot 09_pad4_resting_cursor2
send 1 down; shot 10_pad1_down_cursor3
unplug 4; unplug 3; unplug 1; sleep 0.4
kill -TERM $g 2>/dev/null; wait $g; echo "game exit=$?"
wait
grep -n '\[INPUT\]' "$out/run.log" | cut -c1-140
rm -rf work/save-pad
python3 - "$out" <<'PY'
import sys, glob
from PIL import Image, ImageDraw
s = sorted(glob.glob(sys.argv[1] + '/*.bmp')); W, H = 420, 236; rows = (len(s) + 2) // 3
sh = Image.new('RGB', (W * 3, (H + 18) * rows), 'white'); d = ImageDraw.Draw(sh)
for i, p in enumerate(s):
    x, y = W * (i % 3), (H + 18) * (i // 3)
    sh.paste(Image.open(p).convert('RGB').resize((W, H)), (x, y + 18)); d.text((x + 4, y + 3), p.split('/')[-1][:-4], fill='black')
sh.save(sys.argv[1] + '/sheet.png')
PY
