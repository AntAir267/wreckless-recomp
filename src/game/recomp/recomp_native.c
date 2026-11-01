/**
 * Native replacements for library routines the lifter gets wrong or that are
 * worth running as host code. Each address is listed in
 * config/manual_functions.json, so its generated body is not emitted, and
 * recomp_lookup_manual() (recomp_manual.c) hands out the function here, which
 * is how direct calls reach it too.
 *
 * Calling convention as lifted: on entry g_esp points at the return address,
 * arguments follow it, and the function pops what its `ret` popped.
 */

#include <string.h>

#include "recomp_types.h"

/* memmove / memcpy (the XDK CRT's one routine for both), 0x000F02C0.
 * cdecl (dst, src, n), returns dst. The same code as Double-S.T.E.A.L 2's
 * 0x0011C1B0 (byte for byte to its first branch).
 *
 * Its tail arms sit after an inline jump table, which the lifter does not
 * walk: sp00nznet/wreckless stopped at "[ICALL] Failed to resolve VA
 * 0x000F0418", one of those arms, and a copy through them left its last
 * bytes uncopied. A host memmove is right and faster. */
void sub_000F02C0(void)
{
    uint32_t dst = MEM32(g_esp + 4);
    uint32_t src = MEM32(g_esp + 8);
    uint32_t n   = MEM32(g_esp + 12);

    if (n)
        memmove((void *)XBOX_PTR(dst), (const void *)XBOX_PTR(src), n);
    g_eax = dst;
    g_esp += 4;                         /* ret */
}
