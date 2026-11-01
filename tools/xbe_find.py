"""tools/xbe_find.py VALUE...: where default.xbe holds each 32-bit value (vtables, pointers), as guest addresses."""
import sys, struct
sys.path.insert(0, 'tools')
from xbe_dis import load
d, secs = load('game/Wreckless - The Yakuza Missions/default.xbe')
for a in sys.argv[1:]:
    v = struct.pack('<I', int(a, 16))
    hits = []
    for sva, vs, ra, rs in secs:
        blob = d[ra:ra + rs]; i = blob.find(v)
        while i >= 0:
            hits.append(sva + i); i = blob.find(v, i + 1)
    print(a, ' '.join(f'{h:08X}' for h in hits[:20]))
