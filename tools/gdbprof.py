"""tools/gdbprof.py: a poor man's profiler, gdb as the game's parent (ptrace
scope 1 allows no attaching). Run as

  PROF_GO=flagfile PROF_OUT=out.txt gdb -q -batch -x tools/gdbprof.py --args bin/wreckless ...

The stops come from outside: a loop sending SIGXFSZ (unused by the runtime; SIGINT is its exit) to the game every 0.1 s
(gdb's Python holds its lock while the game runs, so a thread in here never
gets to). Each stop after PROF_GO exists records the stacks of the threads
named in PROF_THREADS (nv2a-ack,guest-main) and lets the game go on; after
PROF_N stops (300) it writes PROF_OUT (self and inclusive counts per
function, per thread) and kills the game."""
import gdb, os, sys, time, collections

OUT = os.environ['PROF_OUT']
GO = os.environ['PROF_GO']
N = int(os.environ.get('PROF_N', '300'))
NAMES = os.environ.get('PROF_THREADS', 'nv2a-ack,guest-main').split(',')
DEPTH = 16

for c in ('set pagination off', 'set confirm off', 'set print thread-events off',
          'set print inferior-events off', 'set width 0', 'set debuginfod enabled off'):
    gdb.execute(c)
for s in ('SIGSEGV SIGBUS SIGUSR1 SIGUSR2 SIGPROF SIGALRM SIGPIPE SIGCHLD SIGILL '
          'SIGFPE SIGWINCH SIGTERM SIGQUIT SIGHUP SIGVTALRM SIGURG SIGIO').split():
    gdb.execute(f'handle {s} nostop noprint pass', to_string=True)
for n in range(32, 65):
    try:
        gdb.execute(f'handle SIG{n} nostop noprint pass', to_string=True)
    except gdb.error:
        pass
gdb.execute('handle SIGINT nostop noprint pass', to_string=True)
gdb.execute('handle SIGXFSZ stop print nopass', to_string=True)  # noprint would imply nostop

gdb.execute('starti', to_string=True)
print('PROF pid', gdb.selected_inferior().pid, file=sys.stderr, flush=True)
taken = [0]

self_c = {n: collections.Counter() for n in NAMES}
incl_c = {n: collections.Counter() for n in NAMES}
stacks = {n: collections.Counter() for n in NAMES}
t0 = time.time()
while True:
    try:
        gdb.execute('continue', to_string=True)
    except gdb.error:
        break
    inf = gdb.selected_inferior()
    if not inf.pid:
        break
    if not os.path.exists(GO):
        continue
    for th in inf.threads():
        name = th.name or ''
        key = next((k for k in NAMES if name.startswith(k)), None)
        if not key:
            continue
        th.switch()
        f = gdb.newest_frame(); names = []
        while f is not None and len(names) < DEPTH:
            names.append(f.name() or hex(f.pc()))
            try:
                f = f.older()
            except gdb.error:
                break
        if names:
            self_c[key][names[0]] += 1
            for nm in set(names):
                incl_c[key][nm] += 1
            stacks[key][' < '.join(names[:6])] += 1
    taken[0] += 1
    if taken[0] % 50 == 0:
        print('PROF stops', taken[0], file=sys.stderr, flush=True)
    if taken[0] >= N:
        break

with open(OUT, 'w') as o:
    o.write(f'{taken[0]} stops in {time.time() - t0:.0f} s\n')
    for k in NAMES:
        tot = sum(self_c[k].values()) or 1
        o.write(f'\n== {k}: self\n')
        for nm, c in self_c[k].most_common(40):
            o.write(f'{100 * c / tot:6.1f}%  {nm}\n')
        o.write(f'\n== {k}: inclusive\n')
        for nm, c in incl_c[k].most_common(40):
            o.write(f'{100 * c / tot:6.1f}%  {nm}\n')
        o.write(f'\n== {k}: stacks\n')
        for st, c in stacks[k].most_common(25):
            o.write(f'{c:5d}  {st}\n')
try:
    gdb.execute('kill', to_string=True)
except gdb.error:
    pass
