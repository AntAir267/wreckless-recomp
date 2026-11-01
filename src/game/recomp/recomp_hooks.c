/**
 * Hooks on the title's own functions: the generated body runs as sub_X_gen,
 * and calls, tail jumps and the dispatch entry reach the sub_X defined here
 * first. Found by the lifter's --exclude-manual scan of this file (regen.sh),
 * so a change to the set of wrapped functions needs a regeneration.
 */

#include <stdio.h>

#include "recomp_types.h"
#include "../options_row.h"
#include "../settings.h"
#include "../pc_page.h"
#include "../widescreen.h"

/* D3D's gamma ramp writer (thiscall 0x000FF634 on the miniport, the ramp
 * on the stack, ret 4): the DAC's palette index to 0, then 256 red, green,
 * blue byte triples to PRMDIO 0x6813C9. The vblank interrupt (sub_001013D0)
 * and the swap (sub_001015B0) call it when the title set a new ramp. On the
 * console the display applies it at scanout; here the port is plain RAM, so
 * the ramp goes to the runtime's presenter (xbox_gamma.h). The game loads
 * only the identity, at start: D3D's SetGammaRamp is not linked in
 * [verified: NOTES.md, gamma]. */
#include "../picture.h"
extern void sub_000FF634_gen(void);
void sub_000FF634(void)
{
    uint32_t ramp = MEM32(g_esp + 4);
    uint8_t rgb[768];
    int i;

    for (i = 0; i < 768; i++)
        rgb[i] = MEM8(ramp + (uint32_t)i);
    wr_picture_game_ramp(rgb);      /* with the TV picture on top (picture.c) */
    sub_000FF634_gen();
}

/* DirectSound's wait for a voice (thiscall 0x0012CD75, the voice in ecx,
 * ret): while bit 0 of the voice's flags word (+0x12) is set, it spins
 * until bit 15 clears. sub_0012D6DC sets bit 15, and the APU's interrupt
 * service clears it (vector 5, 0x0012C3D9 -> ... -> sub_0012CB15). The
 * wait runs in DirectSound's DPC (sub_0012C1EF -> ... -> sub_0012DB98) at
 * DISPATCH_LEVEL; on the console the APU interrupt preempts it. Here an
 * interrupt runs on the gate holder at a safe point, and this tight loop
 * had none, so each pass now runs whatever is pending (xbox_IrqSafePoint
 * saves the guest registers and runs the routine on a worker stack). That
 * alone did not end the freezes: see sub_0012EE6B below. */
void xbox_IrqSafePoint(void);
void xbox_CpuRelax(void);
void sub_0012CD75(void)
{
    uint32_t v = g_ecx;

    if (MEM8(v + 0x12) & 1)
        while (MEM16(v + 0x12) & 0x8000) {
            xbox_IrqSafePoint();
            xbox_CpuRelax();
        }
    g_esp += 4;                         /* ret */
}

/* DirectSound's stream submit (thiscall 0x0012EE6B, the stream voice in
 * ecx, ret 4): it moves queued packets (+0xA8) into a free SSL slot and
 * plays the voice (sub_0012EC76 -> sub_0012DB98), which first waits in
 * sub_0012CD75 for a pending stop (bit 15 of +0x12) to finish. The DPC's
 * service pass (sub_0012C1EF -> sub_0012BCDF -> sub_0012F1C1 ->
 * sub_0012F113) gets here holding the APU object's guard ([+0x8]+0x2C0,
 * raised at 0x0012BCEA), and while the guard is up the interrupt's
 * idle-voice handler (sub_0012C177, cmp at 0x0012C182) ignores the trap
 * that would clear bit 15. A stop pending there never ends: retiring from
 * a mission (the stream flush, sub_0012F033) and mission A-2's end froze
 * with the DPC and the flushing thread both spinning on the same voice
 * [verified: hang reports 20261026-201748, -202648 and -204436, same
 * kernel-timer stack; guard = 1 in the -204436 guest RAM]. The console
 * almost never gets here: the VP traps the off within a 0.67 ms frame and
 * the handler unlinks the voice before the DPC runs, so the pass never sees
 * it. The trap comes later here. So in exactly that state (stop pending,
 * guard up) the submit is skipped; the packets stay queued, and the flush
 * that stopped the voice aborts them once the stop lands. */
extern void sub_0012EE6B_gen(void);
void sub_0012EE6B(void)
{
    uint32_t v = g_ecx;

    if ((MEM16(v + 0x12) & 0x8000) && MEM32(MEM32(v + 0x8) + 0x2C0)) {
        static unsigned held;

        if (++held <= 5 || held % 100 == 0)
            fprintf(stderr, "[DSOUND] stream 0x%08X: submit held, its stop"
                            " is pending under the DPC's guard (%u)\n",
                    v, held);
        g_eax = 0;                      /* nothing submitted */
        g_esp += 8;                     /* ret 4 */
        return;
    }
    sub_0012EE6B_gen();
}

/* The kind of frame, for the menus' own brightness (picture.c): the menu
 * loop (sub_000DF100, every frame of the front end) and the mission loop
 * (sub_000DB330, missions, the attract demo, replays and cutscenes) never
 * run in the same stretch [verified: gdb samples of the game thread in a
 * menu and in mission A-1, 2026-10-26]. */
extern void sub_000DF100_gen(void);
void sub_000DF100(void)
{
    wr_picture_frame(1);
    wr_widescreen_frame(1);
    sub_000DF100_gen();
}
extern void sub_000DB330_gen(void);
void sub_000DB330(void)
{
    wr_picture_frame(0);
    wr_widescreen_frame(0);
    sub_000DB330_gen();
}

/* A screen layout's relocation (cdecl 0x000A68A0, the layout block first;
 * its one caller is the screen loader sub_000A7CB0, which keeps what it
 * returns, the block). For OPTIONS a copy with the PC SETTINGS row is
 * relocated instead (options_row.c). */
extern void sub_000A68A0_gen(void);
void sub_000A68A0(void)
{
    if (wr_pc_setting_row()) {
        uint32_t copy = wr_options_row_patch(MEM32(g_esp + 4));
        if (copy)
            MEM32(g_esp + 4) = copy;
    }
    sub_000A68A0_gen();
}

/* The OPTIONS screen's frame (thiscall 0x000DDDC0, the front end in ecx,
 * ret). Its rows are code: the row at [ecx+0x70], 0 VIBRATION, 1 a
 * CONTROLLER-TYPE row only another language shows, 2 to 4 the volumes,
 * 5 MUSIC TEST, 6 EXIT; up and down (the menu event at 0x001CD50C, 0x0B and
 * 0x0C) move it, then the cursor goes to the layout item 10 x row + the
 * row's choice (sub_000A6F40) with the move sound (sub_000E4310, sound 3);
 * the front end's frame leaves the screen on A (events 1 and 2) at row 6
 * (0x000DECEB) [verified: disassembly]. PC SETTINGS is row 7, between
 * MUSIC TEST and EXIT: up and down to, from and past it are done here, as
 * the screen does its own, and A on it plays the confirm (sound 1, as
 * EXIT's) and opens the PC SETTINGS page (pc_page.c). The event is then
 * cleared for the rest of the frame. Left and right do
 * nothing on it (the screen's switch takes rows 0 to 5). */
extern void sub_000DDDC0_gen(void);
void sub_000DDDC0(void)
{
    uint32_t fe = g_ecx, ev = MEM32(0x001CD50Cu), row = MEM32(fe + 0x70);
    int to = -1;

    if (wr_options_row_on()) {
        if (ev == 0x0C && row == 5)
            to = WR_OPTIONS_PC_ROW;
        else if (ev == 0x0C && row == WR_OPTIONS_PC_ROW)
            to = 6;
        else if (ev == 0x0B && row == 6)
            to = WR_OPTIONS_PC_ROW;
        else if (ev == 0x0B && row == WR_OPTIONS_PC_ROW)
            to = 5;
        else if ((ev == 1 || ev == 2) && row == WR_OPTIONS_PC_ROW) {
            wr_menu_sound(1);
            if (!wr_pc_page_open(fe))
                wr_pc_setting_open();   /* no page in this layout: the overlay */
            MEM32(0x001CD50Cu) = 0;
        }
    }
    if (to >= 0) {
        MEM32(fe + 0x70) = (uint32_t)to;
        wr_cursor_select(fe, (uint32_t)to * 10u);
        wr_menu_sound(3);
        MEM32(0x001CD50Cu) = 0;
    }
    g_ecx = fe;
    sub_000DDDC0_gen();
}

/* The camera's aspect pair for the current view mode (cdecl 0x000213C0,
 * two float pointers, ret): height over width and the width's factor over
 * 4:3; mode 0 (one full view) (0.75, 1), mode 1 (the chase scenes' 16:9
 * panels) (0.5625, 4/3). With game.widescreen every view is shown 4/3
 * wider than the buffer holds it: the pair for that (the full view's is
 * then the panels' own, 16:9) (widescreen.c). */
extern void sub_000213C0_gen(void);
void sub_000213C0(void)
{
    uint32_t a = MEM32(g_esp + 4), b = MEM32(g_esp + 8);

    sub_000213C0_gen();
    if (wr_widescreen_on()) {
        MEMF(a) = MEMF(a) * 0.75f;
        MEMF(b) = MEMF(b) * (4.0f / 3.0f);
    }
}

/* The front end's frame (thiscall 0x000DE940, the front end in ecx, ret):
 * the cursor's input (sub_000A6FC0), then the state's own handling (B and
 * EXIT's A for OPTIONS, state 13, at 0x000DECC6 / 0x000DECEB), then the
 * state's frame function (0x000DF036: OPTIONS' is sub_000DDDC0)
 * [verified: disassembly]. While the PC SETTINGS page is open it takes the
 * menu event first and the rest of the frame sees none, so B goes back to
 * OPTIONS rather than leaving it. */
extern void sub_000DE940_gen(void);
void sub_000DE940(void)
{
    uint32_t fe = g_ecx;
    int page = wr_pc_page_is_open(fe);  /* off unless OPTIONS (state 13) */

    if (page && MEM32(0x001CD50Cu)) {
        wr_pc_page_input(fe, MEM32(0x001CD50Cu));
        MEM32(0x001CD50Cu) = 0;
    }
    g_ecx = fe;
    sub_000DE940_gen();
    if (page)
        wr_pc_page_frame(fe);
}

/* The sprite batch's flush (thiscall 0x0001E0D0, ret): in the menu loop
 * the screen's elements are drawn by it (sub_000DF100: the screen's draw
 * sub_000A6C50 into the batch at 0x00173FC0, then this, then the present)
 * [verified: disassembly]. The PC SETTINGS page's words go over them. */
extern void sub_0001E0D0_gen(void);
void sub_0001E0D0(void)
{
    uint32_t self = g_ecx;

    sub_0001E0D0_gen();
    if (self == 0x00173FC0u)
        wr_pc_page_draw();
}
