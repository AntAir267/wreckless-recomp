"""tools/clean_start.py ADDR... (or - for a JSON seed file on stdin):
for each address, whether code can only arrive there by a branch (the
fork's _starts_clean: a ret, jmp, int3 or padding before it, or a table
end), whether the sweep decoded an instruction starting there, and which
detected function holds it, and whether a switch table names it (case=1). Runs the toolkit's disassembler (--stats-only,
seeds from config/) in-process. Run from the toolkit checkout:
  PYTHONPATH=. ../wreckless-recomp/.venv/bin/python3 ../wreckless-recomp/tools/clean_start.py 0x22A80 ..."""
import bisect, json, os, sys
W = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
addrs = sys.argv[1:]
if addrs == ['-']:
    addrs = [e['start'] for e in json.load(sys.stdin)]
addrs = [int(a, 16) for a in addrs]
from tools.disasm import functions as F
held = {}
orig = F.FunctionDetector.detect_all
def keep(self, sections=None):
    r = orig(self, sections); held['d'] = self; return r
F.FunctionDetector.detect_all = keep
xbe = f"{W}/game/Wreckless - The Yakuza Missions/default.xbe"
argv = ['x', xbe, '--analysis-json', f'{W}/build/xr/analysis.json', '--stats-only', '--force']
for s in ('seed_functions.json', 'seeds_upstream_starts.json'):
    if os.path.exists(f'{W}/config/{s}'):
        argv += ['--seed-functions', f'{W}/config/{s}']
sys.argv = argv
from tools.disasm.__main__ import main
import contextlib, io
with contextlib.redirect_stdout(io.StringIO()):
    try: main()
    except SystemExit: pass
d = held['d']
b = sorted((f.start, f.end, f.name) for f in d.functions.values()); st = [x[0] for x in b]
cases = set()
for t in d.engine.jump_tables:
    cases.update(d.engine.jump_table_entries(t))
for a in addrs:
    i = bisect.bisect_right(st, a) - 1
    where = 'start' if a in d.functions else (f'inside {b[i][2]} (to {b[i][1]:08X})' if i >= 0 and a < b[i][1] else 'gap')
    print(f"{a:08X} clean={int(d._starts_clean(a))} decoded={int(a in d.engine.instructions)} case={int(a in cases)} {where}")
