"""tools/xbe2elf.py XBE OUT.elf: wrap an XBE's sections in an ELF32 i386
executable (one PT_LOAD per section at its address, the real entry point), so
Ghidra (REA) loads it with its own loader and auto-analysis. Only for cross-
checks; nothing runs it. Kernel thunks stay as the XBE has them."""
import struct, sys

d = open(sys.argv[1], 'rb').read()
base = struct.unpack_from('<I', d, 0x104)[0]
entry = struct.unpack_from('<I', d, 0x128)[0] ^ 0xA8FC57AB   # retail key
nsec = struct.unpack_from('<I', d, 0x11C)[0]
sa = struct.unpack_from('<I', d, 0x120)[0] - base
secs = []
for i in range(nsec):
    fl, va, vs, ra, rs, nameaddr = struct.unpack_from('<IIIIII', d, sa + i * 56)
    name = d[nameaddr - base:d.index(b'\0', nameaddr - base)].decode()
    secs.append((name, fl, va, vs, ra, rs))

# The XBE header itself at the base (as the console maps it).
hdr_size = struct.unpack_from('<I', d, 0x108)[0]
loads = [('HEADER', 0, base, hdr_size, 0, hdr_size)] + secs

ph_off, ph_n = 52, len(loads)
data_off = (ph_off + 32 * ph_n + 0xFFF) & ~0xFFF
blobs, phdrs, off = [], [], data_off
for name, fl, va, vs, ra, rs in loads:
    raw = d[ra:ra + rs]
    # Keep file offset congruent with the address mod the page size.
    pad = (va - off) % 0x1000
    off += pad
    blobs.append((off, raw))
    flags = 4 | (1 if (fl & 4) or name in ('.text',) else 0) | (2 if fl & 1 else 0)
    if name == 'HEADER':
        flags = 4
    phdrs.append(struct.pack('<IIIIIIII', 1, off, va, va, len(raw), max(vs, len(raw)), flags, 0x1000))
    off += len(raw)

eh = b'\x7fELF' + bytes([1, 1, 1, 0]) + bytes(8)
eh += struct.pack('<HHIIIIIHHHHHH', 2, 3, 1, entry, ph_off, 0, 0, 52, 32, ph_n, 40, 0, 0)
out = bytearray(off)
out[0:52] = eh
for i, p in enumerate(phdrs):
    out[ph_off + 32 * i:ph_off + 32 * (i + 1)] = p
for o, raw in blobs:
    out[o:o + len(raw)] = raw
open(sys.argv[2], 'wb').write(out)
print(f"entry 0x{entry:08X}, {len(loads)} segments:",
      ', '.join(f"{n}@{va:08X}" for n, _, va, _, _, _ in loads))
