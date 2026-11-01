# Roadmap

From a fresh repo to a release a player would want, in the order that
worked for Double-S.T.E.A.L 2 . The
runtime is that project's (the takedawn fork with every DS2 fix), so most
of what a finished title needs exists already: the Vulkan NV2A renderer,
the enhancements layer (`enhance.toml`), the settings overlay, HDTV and
widescreen reporting, render scale, FXAA/SMAA, anisotropic filtering,
keyboard, rumble, crash and hang reports. What differs is the XDK: 4134,
the launch-era D3D, DSOUND without XACT, WMVDEC (dead code in the US
build). NOTES.md keeps the record of each fix and why.

## 0. Bring-up

- [x] Repo on the DS2 base: regen.sh, CMake, main.c, the runtime on its
      own branch (`wreckless`, a worktree beside DS2's checkout).
- [x] Which XBE: default.xbe is the US game (= DSTEAL_US.xbe).
- [x] Through the generator: 4,268 functions, 0 failed; the fork's
      false function starts fixed, 116 seeds cross-checked against
      upstream v0.16, sp00nznet/wreckless's runs and Ghidra; 4 stubs
      left, all dead code.
- [x] Builds (clang 20, lld).
- [x] First frame, headless: the title screen.
- [x] The attract demo in 3D. It took four runtime fixes for XDK 4134
      (software methods, the DMA_SUBROUTINE register, vertex state
      programs, maskmovq) and one in the disassembler (NOTES.md).
- [ ] The colours of the console: the title is drawn at half intensity
      (to check against a real capture); black blotches in one demo frame.
- [x] Sound: DSOUND 4134 through the emulated APU comes out (recorded
      headless through title, menus and a mission); by ear in section 1.
- [x] Menus with a scripted pad (RECOMP_PAD_SCRIPT / RECOMP_PAD_LIVE): title,
      main menu, scenario, mission and car select, mission level; mission
      A-1 plays (cutscene, driving, HUD, fail, replay).
- [x] Mission speed: 30 fps in real time, as the console (two runtime
      fixes: the software methods' flush wait, the doubled vblank count).

## 1. Play it the way a player would

- [x] Window run (Vulkan present): play sessions, 2026-10-26, from
      the application-menu launcher.
- [x] Controller (the title's XPP driver on the emulated OHCI): a real Hyperkin Duke. [x] The game's own button layout in the README.
- [ ] Sound captured through title, menus and a mission; then by ear.
      heard in play; "audio stopped halfway through a mission" once
      (before the DSOUND guard fix, NOTES.md "The Retire freeze"; not seen
      since, not confirmed fixed).
- [x] Saves: written to the HDD, found again after a restart, loaded;
      dates and times local (headless, runs save9 and load1).
- [ ] Clean exit; no crash report on quit.

## 2. Correctness sweep

- [x] Every menu screen, the garage / car select, options, stats, replay
      mode, save and load (headless).
- [ ] Missions from both scenarios (the A and B maps), their cutscenes,
      results and replays. A-1 and B-1 start and play, a failed A-1 gives
      its replay, Retire gives its replay; played by hand: A-1, A-2 and B-1.
      Freezes at A-2's end and on Retire fixed (DSOUND race, 3a3c013),
      waiting on a play session to confirm. B-1's opening plays headless;
      in the play sessions it ended at once (parked: NOTES.md).
- [ ] The executor's unhandled-method survey and skipped draws, read for
      4134-specific gaps (software methods, the swap through the CRTC).
- [x] An ABI-check build (-DRECOMP_ABI_CHECK, as DS2 used) over a long
      scripted run: only CRT helpers, by design (NOTES.md).
- [ ] Frames against press screenshots of the release.

## 3. Rendering enhancements, the game's own features first

- [x] What video modes the game offers: none (640x480 4:3 only; it never
      reads the dashboard's video flags). Widescreen = a game patch, later.
- [x] Render scale 2, anisotropic filtering 16, as DS2's defaults.
- [ ] Draw distance, if the game's LODs allow it.
- [ ] No AI-upscaled textures.

## 4. Quality of life

- [x] The runtime's settings overlay built in (Back + Start, F1;
      src/game/settings.c hides what this game cannot use).
- [x] Settings in the game's own OPTIONS screen, in its style :
      OPTIONS > PC SETTINGS, a page of the screen in its own font with the
      port's main settings, MORE SETTINGS (the overlay for the rest), QUIT
      GAME and EXIT (NOTES.md, "The PC SETTINGS page").
- [x] Settings in the game: PC SETTINGS, a row on the game's own OPTIONS
      screen that opens the runtime's overlay (options_row.c; the layout is
      an earlier version of DS2's, NOTES.md "PC SETTINGS in OPTIONS").
- [ ] Rumble: the game's own, checked on a pad.
- [ ] Language: the US build is English only; the JP build is a separate
      program (see 5).
- [ ] 60 fps: only if the logic is not frame-stepped (it was in DS2).

## 5. Project

- [x] The runtime as antbox, the shared Xbox toolkit (AntAir267/antbox): takedawn plus every
      game-agnostic change, as reviewable commits.
- [x] This game builds from ../antbox at
      ANTBOX_PIN and runs its check on each candidate (NOTES.md, Layout).
- [x] README for players and for building (the full button layout after
      a play test).
- [x] package.sh and a Steam Deck build (deck-build.sh, the podman
      container, glibc 2.38), as DS2's.
- [ ] Maybe later: the original Double-S.T.E.A.L (DSTEAL_JP.xbe) as a
      second build (WR_XBE=DSTEAL_JP.xbe; its own seeds; its WMV intro).

## 5b. Remove the overhead 

Measure, then cut the layers; never change the game's timing; the checks
(chk_demo, chk_mis) must still pass. Numbers so far, A-1 with the
scripted drive (RECOMP_TRACE=frametime, work/ft-chk_mis.csv, 2026-10-26,
Win Max 2, render scale 2):
- Frame 33.4 ms (the game's 30 fps), p99 33.6 ms.
- Process CPU 51.6 ms a frame: 1.5 cores busy for a 733 MHz console.
- Executor (push buffer to Vulkan) busy 14.1 ms a frame, idle 8.1.
- Guest samples: D3D 22.5 ms a frame (the launch-era D3D building push
  buffers), game 6.4, XDK 1.2, host 0.7, waiting 69.
- GPU time and load times not measured yet (gpu_ms reads -1 here).
- [ ] GPU time and mission load time measured and recorded.
- [ ] Where D3D's 22.5 ms goes (sampling profile of the game thread), and
      the executor's 14 ms; safe wins first.
      First lead (antbox's maintainer, from DS2, 2026-10-27): DS2's main thread
      was 90% the XDK D3D KickOff wait, a tight `test [0x100410], 0x10000;
      jne` loop; listed in its config/spin_waits.json (--spin-waits) and
      woken by antbox's kick_cleared -> xbox_SpinWake, guest-main went
      98% -> 10% (process 104% -> 67%) under Balanced pacing
      (Double-S.T.E.A.L 2's notes). Wreckless's 4134 D3D
      likely has the same loop on its main thread (the 22.5 ms above
      [guess]); sub_000FE6A0 is the same bit's spin on the executor side
      (NOTES.md, Mission speed). Done 2026-10-28: KickOff's wait lowered
      (config/spin_waits.json) and Balanced pacing named in enhance.toml:
      process 144% -> 116-124% of a core, main thread 97% -> 43%, frames as
      steady (NOTES.md, "Overhead").
- [ ] Pacing: antbox's next batch (0bd4ab3) gives a port that names no
      present.pacing Balanced (sleep, 8 ms lead) instead of the busy-wait;
      Wreckless names none. Compare frame times (tools/check.sh's chk_mis)
      on that batch's check.

- [ ] The native Vulkan renderer (decided 2026-10-29): built once in
      antbox by antbox's maintainer for all the Xbox games, switchable, the
      current path the default until it has been played. This project only
      tests it for Wreckless when asked; no renderer of its own here.

## 6. Enhanced graphics

The project's goals: small ones any time (widescreen, resolution,
filtering); the heavy ones once the game plays start to finish at its
defaults with no known crashes. Each its own option, on or off by default
as suits the game, the original look always reachable, and what is on by
default at full frame rate on the Win Max 2. Ideas:
- [x] Per-fragment W-buffer depth (toolkit 6b75477, in antbox's maintainer's
      next batch): the play report's "objects in the wrong order". Depth and motion
      data kept available for later passes is still to plan.
- [x] Widescreen (game.widescreen, on): the camera's own 16:9 pair, the
      frame shown 16:9 in missions, the HUD unstretched (toolkit
      nv2a_hud_squeeze). No measurable cost (NOTES.md, Widescreen).
- [ ] Ambient occlusion, better shadows (the 512x512 shadow map at a higher
      resolution), volumetric fog and light shafts, ray-traced shadows and
      reflections where the GPU has it.
- [ ] Draw distance, if the LODs allow it.

The defaults render.scale 2 and graphics.aniso 16 (section 3) fit them:
small, on by default, and the console's picture is one setting away.

## Controllers

- [x] Any connected controller drives player 1, in any order, late or after an
      unplug (NOTES.md, Controllers).
      tools/pad_test.sh is the check.
- [x] The buttons in the README, from the 2002 FAQ. Black and Back unknown.

