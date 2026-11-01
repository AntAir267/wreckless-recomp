"""tools/gen_seeds.py: config/seeds_crosscheck.json from the cross-checks.

Candidates: function starts upstream xboxrecomp v0.16 finds (build/xr-up),
indirect-call targets sp00nznet/wreckless's runs observed on default.xbe
(work/upstream_seeds_default.json, its intro-video branch) and functions
Ghidra finds (work/ghidra/functions.json, REA on tools/xbe2elf.py's wrapper),
that the fork's disassembler does not. Kept: those tools/clean_start.py
calls clean, decoded and not a switch case (only a branch can arrive: a ret, jmp, int3 or padding before
it) and decoded (an instruction starts there). A clean start inside a
detected body is kept unless that body branches to it itself (a case or
tail of its own, already lifted inline). Input: work/crosscheck.txt, the
clean_start.py report on work/crosscheck_candidates.json."""
import json
xr = json.load(open('build/xr/disasm/xrefs.json'))
fn = json.load(open('build/xr/disasm/functions.json'))
bodies = {int(f['start'], 16): int(f['end'], 16) for f in fn}
src = {int(e['start'], 16): e['src'] for e in json.load(open('work/crosscheck_candidates.json'))}
internal = {}
for r in xr:
    if r['type'] in ('jump', 'cond_jump'):
        internal.setdefault(int(r['to'], 16), []).append(int(r['from'], 16))
out, skipped = [], []
for line in open('work/crosscheck.txt'):
    p = line.split()
    if len(p) < 4 or not p[1].startswith('clean='):
        continue
    a = int(p[0], 16)
    if p[1] != 'clean=1' or p[2] != 'decoded=1':
        skipped.append((a, 'not clean' if p[1] != 'clean=1' else 'not decoded'))
        continue
    if p[3] == 'case=1':
        skipped.append((a, 'a case of a switch table, lifted inline'))
        continue
    p.pop(3)
    if p[3] == 'inside':
        name = p[4]; s = int(name[4:], 16); e = bodies[s]
        if any(s <= f < e for f in internal.get(a, [])):
            skipped.append((a, f'a branch target of {name} itself'))
            continue
    out.append({'start': f'0x{a:08X}',
                'note': f"{src[a]}; clean start ({'in a gap' if p[3] == 'gap' else 'inside ' + p[4] + ', which never branches to it'})"})
json.dump(out, open('config/seeds_crosscheck.json', 'w'), indent=1)
print(len(out), 'seeds;', len(skipped), 'skipped')
for a, why in skipped:
    print(f'  skip {a:08X} {src[a]}: {why}')
