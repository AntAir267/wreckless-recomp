#!/usr/bin/env bash
# tools/dis.sh VA [BYTES]: disassemble default.xbe's code at a guest address,
# through the section table (each section has its own file offset).
va=$(( $1 )); n=$(( ${2:-256} ))
x="${WR_XBE_PATH:-game/Wreckless - The Yakuza Missions/default.xbe}"
delta=$(python3 - "$x" "$va" <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read(); va = int(sys.argv[2])
base = struct.unpack_from('<I', d, 0x104)[0]; n = struct.unpack_from('<I', d, 0x11C)[0]
sa = struct.unpack_from('<I', d, 0x120)[0] - base
for i in range(n):
    fl, sva, vs, ra, rs = struct.unpack_from('<IIIII', d, sa + i * 56)
    if sva <= va < sva + rs:
        print(sva - ra); break
PY
)
[ -n "$delta" ] || { echo "0x$(printf %X $va) is in no section's raw data"; exit 1; }
objdump -D -b binary -mi386 -M intel --adjust-vma=$delta \
    --start-address=$va --stop-address=$((va + n)) "$x" | sed -n '8,$p'
