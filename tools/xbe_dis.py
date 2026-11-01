import sys, struct
def load(path):
    d = open(path,'rb').read()
    base = struct.unpack_from('<I', d, 0x104)[0]
    nsec = struct.unpack_from('<I', d, 0x11C)[0]
    secaddr = struct.unpack_from('<I', d, 0x120)[0] - base
    secs = []
    for i in range(nsec):
        o = secaddr + i*56
        fl, va, vs, ra, rs = struct.unpack_from('<IIIII', d, o)
        secs.append((va, vs, ra, rs))
    return d, secs
def read(d, secs, va, n):
    for sva, vs, ra, rs in secs:
        if sva <= va < sva+rs:
            off = ra + (va - sva)
            return d[off:off+n]
    return b''
if __name__ == '__main__':
    d, secs = load(sys.argv[1])
    va = int(sys.argv[2], 16); n = int(sys.argv[3], 0) if len(sys.argv) > 3 else 120
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for i in md.disasm(read(d, secs, va, n), va):
        print(f"{i.address:08X}  {i.bytes.hex():<20} {i.mnemonic} {i.op_str}")
