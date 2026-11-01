"""tools/gendiff.py OLD_GEN NEW_GEN: which generated functions were added,
removed or changed between two regenerations (bodies compared whole; the
split into files does not matter)."""
import glob, re, sys

def funcs(d):
    out = {}
    for p in sorted(glob.glob(f'{d}/recomp_*.c')):
        cur, buf = None, []
        for line in open(p, errors='replace'):
            m = re.match(r'^void (sub_[0-9A-F]{8}(?:_gen)?)\(void\)', line)
            if m and not line.rstrip().endswith(';'):
                cur, buf = m.group(1), [line]
                continue
            if cur:
                buf.append(line)
                if line.startswith('}'):
                    out[cur] = ''.join(buf); cur = None
    return out

a, b = funcs(sys.argv[1]), funcs(sys.argv[2])
added = sorted(b.keys() - a.keys()); removed = sorted(a.keys() - b.keys())
changed = sorted(k for k in a.keys() & b.keys() if a[k] != b[k])
print(f'{len(a)} -> {len(b)} functions: {len(added)} added, {len(removed)} removed, {len(changed)} changed')
for name, lst in (('added', added), ('removed', removed), ('changed', changed)):
    if lst:
        print(f'{name}:', ' '.join(lst))
