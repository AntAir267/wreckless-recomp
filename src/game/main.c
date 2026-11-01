/**
 * Wreckless: The Yakuza Missions - recompiled game entry point
 *
 * The native program that hosts the recompiled game code. Boot order is the
 * Double-S.T.E.A.L 2 recomp's, itself the toolkit
 * template's and sp00nznet/wreckless's, on a POSIX host:
 *
 *   1. load default.xbe
 *   2. map the Xbox address space (data sections at their original VAs)
 *   3. the emulated APU, USB host controllers, kernel, paths, kernel bridge
 *   4. guest stack, flat dispatch table, hang watchdog
 *   5. call the recompiled entry point
 *
 * XBE (xbe_parser):
 *   Title:       WRECKLESS (North America; byte for byte DSTEAL_US.xbe)
 *   Title ID:    0x4156000A
 *   Built:       2002-01-08, XDK 1.0.4134
 *   Debug path:  D:\Projects\DSTEAL02\Release\DSTEAL.exe
 *   Base addr:   0x00010000
 *   Entry point: 0x000EB57E
 *   Code:        .text 926 KB, plus D3D, D3DX, XGRPH, DSOUND, WMVDEC, XPP
 *                and DOLBY library sections
 *   Kernel imports: 113
 *
 * Usage: wreckless [--game-dir DIR] [--save-dir DIR] [--headless]
 *   --game-dir  folder holding default.xbe and the disc files
 *               (default: "game/Wreckless - The Yakuza Missions")
 *   --save-dir  the Xbox hard disk's save folders
 *               (default: $XDG_DATA_HOME/wreckless)
 */

#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ucontext.h>
#include <unistd.h>
#include <dlfcn.h>
#include <execinfo.h>

#include <xbox/xboxrecomp.h>
#include "video/video_player.h"     /* xbox_HostWindowMain */
#include "usb/ohci.h"               /* xbox_OhciInit */
#include "platform/recomp_env.h"    /* recomp_env */
/* The takedawn branch's crash and hang reports: <save dir>/crash, with every
 * registered thread sampled when no frame has flipped for a while
 * (RECOMP_HANG_WATCHDOG, default 15 s). */
#if defined(__has_include)
#if __has_include("xbox_crash.h")
#include "xbox_crash.h"
#define WR_HAVE_CRASH_REPORTS 1
#endif
#endif
/* The opt-in enhancements layer (CMakeLists.txt): enhance.toml beside the
 * executable. */
#if defined(WR_HAVE_ENHANCE)
#include "enhance.h"                /* xbox_enhance_init */
#include "enhance_cfg.h"            /* enhance_cfg_report_unused */
#include "platform/recomp_exe_dir.h"
#endif
#include "platform/recomp_restart.h"   /* recomp_restart_init */
#include "settings.h"                  /* the settings overlay's side of this title */
#include "widescreen.h"                /* game.widescreen */

/* RECOMP_TLS is required: the runtime defines the guest registers
 * thread-local, and a plain extern resolves to the TLS template rather than
 * this thread's copy (wreckless found this the hard way). */
extern RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
extern RECOMP_TLS uint32_t g_ebx, g_esi, g_edi;
extern ptrdiff_t g_xbox_mem_offset;
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;

#define WR_ENTRY_POINT      0x000EB57Eu
#define WR_GAME_DIR         "game/Wreckless - The Yakuza Missions"
/* D3D's global device pointer (XDK 4134 D3D8, in the D3D section's BSS):
 * every D3D entry reads it, e.g. 0x000F8764 "mov eax, [0x0010A050]". */
#define WR_D3D_DEVICE_PTR   0x0010A050u

extern void xbe_entry_point(void);
int recomp_dispatch_init(void);

typedef struct MCPXAPUState MCPXAPUState;
extern MCPXAPUState *mcpx_apu_init_standalone(uint8_t *ram_ptr);
extern MCPXAPUState *g_apu_state;

static char s_game_dir[PATH_MAX];
static char s_save_dir[PATH_MAX];
static char s_xbe_path[PATH_MAX + 16];

/* ── crash report ──────────────────────────────────────────── */

/* The guest's call chain: every lifted call pushes its guest return address
 * onto the guest stack, so scanning up from esp for values inside the code
 * sections recovers it (over-reports; these are FPO frames). */
static void print_guest_stack(void)
{
    const uint32_t *sp;
    int i, shown = 0;

    if (!g_xbox_mem_offset || !g_esp)
        return;
    sp = (const uint32_t *)((uintptr_t)g_xbox_mem_offset + g_esp);
    fprintf(stderr, "  guest stack (return addresses, innermost first):\n");
    for (i = 0; i < 256 && shown < 24; i++) {
        uint32_t v = sp[i];
        if (v > g_xbox_code_lo && v < g_xbox_code_hi) {
            fprintf(stderr, "    [esp+%-4d] 0x%08X\n", i * 4, v);
            shown++;
        }
    }
    if (!shown)
        fprintf(stderr, "    (no return addresses in range)\n");
    fprintf(stderr, "  recent ICALL targets:");
    for (i = 0; i < 16; i++)
        fprintf(stderr, " %08X", g_icall_trace[(g_icall_trace_idx + i) & 15]);
    fprintf(stderr, "\n");
}

static void on_fault(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = ucv;
    uintptr_t fault = (uintptr_t)si->si_addr;
    void *rip = NULL;
    Dl_info di;

    /* Trapped hardware registers (APU, AC'97, USB OHCI): serviced, and the
     * program counter stepped past the access. */
    if ((sig == SIGSEGV || sig == SIGBUS) && xbox_PosixMmioFault(ucv, fault))
        return;

#if defined(__x86_64__)
    rip = (void *)(uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
#endif
    fprintf(stderr, "\n[CRASH] signal %d at host pc %p, fault addr %p\n",
            sig, rip, si->si_addr);
    if (rip && dladdr(rip, &di) && di.dli_sname)
        fprintf(stderr, "  in %s+0x%lx\n", di.dli_sname,
                (unsigned long)((uintptr_t)rip - (uintptr_t)di.dli_saddr));
    fprintf(stderr, "  guest regs: eax=%08X ecx=%08X edx=%08X esp=%08X\n",
            g_eax, g_ecx, g_edx, g_esp);
    fprintf(stderr, "              ebx=%08X esi=%08X edi=%08X\n",
            g_ebx, g_esi, g_edi);
    if (g_xbox_mem_offset && fault >= (uintptr_t)g_xbox_mem_offset)
        fprintf(stderr, "  guest VA of fault: 0x%08llX\n",
                (unsigned long long)(fault - (uintptr_t)g_xbox_mem_offset));
    print_guest_stack();
    {
        void *bt[32];
        int n = backtrace(bt, 32);
        fprintf(stderr, "  host backtrace:\n");
        backtrace_symbols_fd(bt, n, 2);
    }
    fflush(stderr);
    signal(sig, SIG_DFL);
    raise(sig);
}

static void install_fault_handler(void)
{
    /* Its own stack: a guest stack overflow or a runaway copy can leave the
     * host stack unusable at exactly the moment the report is needed. */
    static uint8_t alt[256 * 1024];
    stack_t ss = { .ss_sp = alt, .ss_size = sizeof alt };
    struct sigaction sa;

    sigaltstack(&ss, NULL);
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_fault;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
}

/* ── boot ──────────────────────────────────────────────────── */

static bool load_xbe(const char *path, void **out_data, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    long size;
    void *data;

    if (!f) {
        fprintf(stderr, "Cannot open XBE: %s (%s)\n", path, strerror(errno));
        return false;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || !(data = malloc((size_t)size))) {
        fclose(f);
        return false;
    }
    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return false;
    }
    fclose(f);
    *out_data = data;
    *out_size = (size_t)size;
    return true;
}

static int boot_and_run(void)
{
    void *xbe_data = NULL;
    size_t xbe_size = 0;

    /* Here, not in main: the alternate signal stack belongs to the thread
     * that runs the guest (xbox_HostWindowMain may start a new one). */
    install_fault_handler();
#if defined(WR_HAVE_CRASH_REPORTS)
    xbox_crash_init(s_save_dir);
    xbox_crash_thread_register("host-main");
#endif

    printf("=== Wreckless: The Yakuza Missions - Static Recompilation ===\n");
    printf("game dir: %s\nsave dir: %s\n", s_game_dir, s_save_dir);

    if (!load_xbe(s_xbe_path, &xbe_data, &xbe_size))
        return 1;
    printf("XBE loaded: %zu bytes\n", xbe_size);

    if (!xbox_MemoryLayoutInit(xbe_data, xbe_size)) {
        fprintf(stderr, "Xbox memory layout init failed "
                        "(required VA range unavailable?)\n");
        free(xbe_data);
        return 1;
    }
    g_xbox_mem_offset = xbox_GetMemoryOffset();
    printf("Xbox memory mapped at host offset 0x%llX\n",
           (unsigned long long)g_xbox_mem_offset);

    /* The emulated APU walks Xbox physical RAM for voice buffers. Gated on
     * the variable that unmaps its registers: the two halves are useless
     * apart. Wreckless skipped its whole engine init when DirectSoundCreate
     * failed (docs/pipeline/06-debugging.md, "Runs, Never Draws a Frame"),
     * so this one matters for more than sound. */
    if (recomp_env(RENV_AC97_READY) && strcmp(recomp_env(RENV_AC97_READY), "0")) {
        g_apu_state = mcpx_apu_init_standalone((uint8_t *)xbox_GetMemoryBase());
        fprintf(stderr, "[BOOT] emulated APU %s\n",
                g_apu_state ? "up" : "FAILED to initialise");
    }

    /* XAPI is statically linked and drives the OHCI registers itself, so a
     * pad only exists if the host controllers do. */
    xbox_OhciInit();

    xbox_kernel_init();
    mkdir(s_save_dir, 0755);
    xbox_path_init(s_game_dir, s_save_dir);
    xbox_kernel_bridge_init();

    /* The GPU fence D3D waits on. sub_000FE8A0 reserves pushbuffer space
     * and spins until the GPU has consumed enough of it (0x000FE912):
     *
     *     mov edx, [edi+0x34]   ; -> what the GPU has consumed
     *     mov edi, [edi+0x30]   ;    what the title has submitted
     *  L: mov ecx, [edx] ... jb L
     *
     * XDK 4134 keeps the pair one dword later than 5849's (0x2C, 0x30), as
     * sp00nznet/wreckless found. Re-read and bounds-checked on every poll,
     * so registering before the device exists is harmless. */
    xbox_Nv2aMirrorFence(WR_D3D_DEVICE_PTR, 0x30u, 0x34u);

    /* The vblank count: D3D's GetDisplayFieldStatus (sub_000FAF80) hands
     * back [device+0x245C], which the vblank interrupt moves
     * (sub_001013D0, [miniport+0x1F4] with the miniport at device+0x2268),
     * and the game's frame wait (sub_000DC5E0, sp00nznet/wreckless) spins
     * until it has moved by two. The runtime stops bumping it once the
     * title moves it. */
    xbox_Nv2aFrameCounter(WR_D3D_DEVICE_PTR, 0x245Cu);

#if defined(XBOXRECOMP_HAVE_VULKAN)
    /* D3D software methods (NO_OPERATION with a parameter). XDK 4134's
     * PGRAPH service (sub_00101850) hands a trapped 0x100 to sub_001015B0
     * (thiscall on the miniport object, device+0x2268; ret 8: the method,
     * then its data from PGRAPH 0x1A88), and its swap is one of them: it
     * points the CRTC at the new front buffer (sub_000FF5B0). The runtime's
     * 4134 mode passes the data (kernel_bridge.c). */
    xbox_Nv2aSetSoftwareMethodHandler4134(0x001015B0u, WR_D3D_DEVICE_PTR, 0x2268u);
#endif

    g_esp = XBOX_STACK_TOP;

    if (!recomp_dispatch_init())
        fprintf(stderr, "[BOOT] flat dispatch unavailable; "
                        "indirect calls will use the binary search\n");

    /* From this thread: the guest registers it samples are thread-local.
     * Inert unless RECOMP_WATCHDOG_SECS is set. */
    xbox_WatchdogStart();
#if defined(WR_HAVE_CRASH_REPORTS)
    xbox_crash_watchdog_start();
#endif

    printf("\nStarting game at entry point 0x%08X...\n", WR_ENTRY_POINT);
    fflush(stdout);
    xbe_entry_point();

    printf("\nGame returned. Cleaning up...\n");
    xbox_kernel_shutdown();
    xbox_MemoryLayoutShutdown();
    free(xbe_data);
    return 0;
}

/* A runtime setting unless the environment (or settings.toml) already
 * names one; also the overlay's default for it. */
static void env_default(const char *name, const char *value)
{
    setenv(name, value, 0);
    wr_settings_default(name, value);
}

/* What this title needs from the runtime, found in bring-up (NOTES.md).
 * Set before anything in the runtime reads its settings. */
static void runtime_defaults(void)
{
    /* DirectSound checks the AC'97 codec and then drives the APU itself;
     * a failed probe skips the engine's whole init (sp00nznet/wreckless:
     * sub_000E43E0 initialises everything behind DirectSoundCreate). */
    env_default("RECOMP_AC97_READY", "1");
    /* D3D's vblank interrupt: its handler moves the field count the frame
     * wait reads and completes the vsync'd swaps. */
    env_default("RECOMP_VBLANK", "1");
    /* The NV2A pushbuffer executor: without it nothing is drawn. */
    env_default("RECOMP_PB_EXEC", "1");
#if defined(XBOXRECOMP_HAVE_VULKAN)
    /* The takedawn branch's Vulkan NV2A renderer, as Takedawn uses it on
     * Linux and the Steam Deck; flips held to the emulated vblank. */
    env_default("RECOMP_PB_BACKEND", "vulkan");
    env_default("RECOMP_FLIP_PACING", "vblank");
    /* The CPU reads the two 16x16 targets at the ends of the 64-32-16
     * chains back each frame, for the exposure of the 3D (as DS2's
     * 16x16). Unread, a mission was drawn at about half the brightness
     * the console's is (mean 36 against 64; a World of Longplays capture
     * of the Xbox game measures 51-71) [verified: runs chk_mis, rb1]. */
    env_default("RECOMP_GPU_CPU_READS", "16x16");
#else
    env_default("RECOMP_TITLE_KEVENTS", "1");
#endif
    /* The DSP doorbell ack and the emulated USB controller are debug-tier
     * keys: their defaults are in env/recomp_env_game.h. */
}

static void absolute(char *out, size_t n, const char *path)
{
    char cwd[PATH_MAX];

    if (path[0] == '/' || !getcwd(cwd, sizeof cwd))
        snprintf(out, n, "%s", path);
    else
        snprintf(out, n, "%s/%s", cwd, path);
}

int main(int argc, char **argv)
{
    const char *game = WR_GAME_DIR, *save = NULL;
    int i;

    /* First: the arguments and the environment as given, so a restart
     * starts the program as it was. */
    recomp_restart_init(argc, argv);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--game-dir") && i + 1 < argc)
            game = argv[++i];
        else if (!strcmp(argv[i], "--save-dir") && i + 1 < argc)
            save = argv[++i];
        else if (!strcmp(argv[i], "--headless"))
            setenv("RECOMP_HEADLESS", "1", 1);
        else {
            fprintf(stderr, "usage: %s [--game-dir DIR] [--save-dir DIR] [--headless]\n",
                    argv[0]);
            return 2;
        }
    }
    absolute(s_game_dir, sizeof s_game_dir, game);
    if (save) {
        absolute(s_save_dir, sizeof s_save_dir, save);
    } else {
        const char *xdg = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
        if (xdg && *xdg)
            snprintf(s_save_dir, sizeof s_save_dir, "%s/wreckless", xdg);
        else
            snprintf(s_save_dir, sizeof s_save_dir, "%s/.local/share/wreckless",
                     home ? home : ".");
    }
    snprintf(s_xbe_path, sizeof s_xbe_path, "%s/default.xbe", s_game_dir);

    /* The player's settings.toml (the overlay's, in the save folder) first:
     * what it sets goes into the environment, over every default below. */
    wr_settings_init(s_save_dir);
    runtime_defaults();
#if defined(WR_HAVE_ENHANCE)
    /* After the defaults (the environment wins over the file, the file over
     * the stock values), before anything starts the guest or a backend. */
    xbox_enhance_init(recomp_exe_dir(), NULL);
    /* The title's own key ([game]); the environment wins over the file. */
    enhance_cfg_bind_env("game.tv_picture", RENV_TV_PICTURE);
    enhance_cfg_bind_env("game.menu_brightness", RENV_MENU_BRIGHTNESS);
    enhance_cfg_bind_env("game.widescreen", RENV_WIDESCREEN);
    /* enhance.toml's values as the overlay's defaults. */
    wr_settings_finish();
    enhance_cfg_report_unused();
#endif

    /* The frame shape, before the window opens at it. */
    wr_widescreen_init();

    /* With RECOMP_FB_WINDOW set this opens the framebuffer window on the
     * main thread and runs the boot on a second one; otherwise it just
     * calls boot_and_run. */
    return xbox_HostWindowMain(boot_and_run);
}
