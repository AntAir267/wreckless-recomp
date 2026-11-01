"""tools/pbdecode.py DUMP ADDR [WORDS] [--from ADDR2]: decode NV2A pushbuffer
words from a contiguous-memory dump (the executor's DS2_PB_STOP_DUMP file:
64 MB, physical 0 at offset 0). ADDR is a GPU address (0x80xxxxxx or
physical). Prints control words (jump, call, return) and method headers
with their parameters, the way the NV2A's DMA pusher reads them.
--from starts decoding at ADDR2 (in step) and prints from ADDR on."""
import struct, sys

NAMES = {0x0100: 'NO_OPERATION', 0x0110: 'WAIT_FOR_IDLE', 0x0130: 'FLIP_STALL',
         0x012C: 'FLIP_INCREMENT_WRITE', 0x1D8C: 'ZSTENCIL_CLEAR_VALUE',
         0x1D90: 'COLOR_CLEAR_VALUE', 0x17FC: 'SET_BEGIN_END', 0x1800: 'ARRAY_ELEMENT16',
         0x1808: 'ARRAY_ELEMENT32', 0x1810: 'DRAW_ARRAYS', 0x1818: 'INLINE_ARRAY',
         0x1D6C: 'SEMAPHORE_OFFSET', 0x1D70: 'BACK_END_WRITE_SEMAPHORE_RELEASE',
         0x0210: 'SURFACE_COLOR_OFFSET', 0x0B00: 'TRANSFORM_PROGRAM',
         0x1720: 'VERTEX_DATA_ARRAY_OFFSET', 0x1760: 'VERTEX_DATA_ARRAY_FORMAT'}

def main():
    args = sys.argv[1:]
    start = None
    if '--from' in args:
        i = args.index('--from'); start = int(args[i + 1], 16); del args[i:i + 2]
    m = open(args[0], 'rb').read()
    show = int(args[1], 16) & 0x0FFFFFFF
    n = int(args[2], 0) if len(args) > 2 else 64
    va = (start & 0x0FFFFFFF) if start is not None else show
    left = 0; method = 0; noninc = False; printed = 0
    while printed < n and va + 4 <= len(m):
        w = struct.unpack_from('<I', m, va)[0]
        out = show <= va < show + 4 * n + 0x40
        at = va; va += 4
        if left:
            if out:
                print(f'  {0x80000000 | at:08X}  {w:08X}    {NAMES.get(method, hex(method))} = {w:#x}')
            if not noninc: method += 4
            left -= 1
        elif (w & 3) == 1 or (w & 0xE0000003) == 0x20000000:
            t = w & ~3 if (w & 3) == 1 else w & 0x1FFFFFFC
            if out: print(f'{0x80000000 | at:08X}  {w:08X}  JUMP -> {0x80000000 | t:08X}')
            if start is None: break
            va = t
        elif (w & 3) == 2:
            if out: print(f'{0x80000000 | at:08X}  {w:08X}  CALL -> {0x80000000 | (w & ~3):08X}')
            if start is None: break
            va = w & ~3
        elif w == 0x00020000:
            if out: print(f'{0x80000000 | at:08X}  {w:08X}  RETURN')
            if start is None: break
        elif (w & 0xE0030003) in (0, 0x40000000):
            left = (w >> 18) & 0x7FF; sub = (w >> 13) & 7; method = w & 0x1FFC
            noninc = (w & 0xE0000000) == 0x40000000
            if out: print(f'{0x80000000 | at:08X}  {w:08X}  sub{sub} {NAMES.get(method, hex(method))} x{left}{" (non-inc)" if noninc else ""}')
        else:
            if out: print(f'{0x80000000 | at:08X}  {w:08X}  ?? unknown')
        if out: printed += 1

main()
