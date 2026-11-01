# Working notes

Claims are marked [verified] (with the evidence) or [guess] (not checked
yet); don't build on a [guess] without checking it.

## Layout

- `game/Wreckless - The Yakuza Missions/` — the disc's files (1.7 GB, not in
  git): default.xbe, every regional XBE and data/. Copied from the Double-
  S.T.E.A.L 2 recomp's `work/wreckless-usa/game` (extracted there from the owner's Redump image, "Wreckless - The
  Yakuza Missions (USA).7z").
- `../antbox` — the runtime and toolkit: [AntAir267/antbox](https://github.com/AntAir267/antbox)
  at the commit in ANTBOX_PIN. A new toolkit commit gets this game's check
  (tools/check.sh) and, if it passes, a new pin:
    git -C ../antbox checkout <commit>
    (./regen.sh if tools/recomp or tools/disasm changed; tools/gendiff.py)
    ninja -C build/linux
    EVERY=120 tools/run.sh chk_demo 60    title, then the demo in 3D
    tools/mission_drive.sh chk_mis 110    A-1 at 33.3 ms frames
  Toolkit commit hashes in these notes are from its private working repo,
  which came before the public one.
- `tools/` — dis.sh / xbe_dis.py / xbe_find.py (code and values at guest
  addresses, through the section table), xbe2elf.py (the XBE as an ELF for
  Ghidra), clean_start.py and gen_seeds.py (the seed cross-check), gendiff.py
  (which generated functions a regeneration changed), run.sh (a headless run
  with frame dumps in work/runs/NAME), pbdecode.py (pushbuffer words from a
  stop dump), gdbprof.py (stack samples of the executor and the game
  thread, gdb as the game's parent).
  Frame dumps are numbered by count, and a mid-frame surface dump takes a
  number from the same sequence, so fNNN.bmp is not always the NNNth flip
  dump. Since antbox 7faffb0 each dump logs "[GPU] dump .../fNNN.bmp: frame
  at flip N"; pair by that line. (An earlier note here said dumps lag their
  flip: wrong, corrected by antbox's maintainer 2026-10-30, whose replays of
  this game's captures match the live dumps 25 of 25 when paired so.)
- `work/` — scratch, not in git: sysroot/ (SDL2, glslang, SPIRV-Tools dev
  files, copied from DS2's), ghidra/ (the ELF wrapper and Ghidra's function
  list), the upstream seed snapshot, regen and build logs, runs.
- `../work/wreckless-upstream` — sp00nznet/wreckless. main is 9e35c2c; the
  branch `work/intro-video-and-attract-mode` (d0e054b, 2026-10-07) is later
  and matters more (below).

## The disc's XBEs [verified: md5]

default.xbe is byte for byte DSTEAL_US.xbe: the US game itself, not a region
launcher. So the build is the US English game; nothing to settle there.
UK is a different build of the same size; FR/GR are larger (1.9 MB); JP
(1.5 MB) is the original Double-S.T.E.A.L, JPDEMO its demo.

| | default.xbe (US) |
|---|---|
| Title | WRECKLESS, 0x4156000A, North America |
| Built | 2002-01-08, XDK 1.0.4134 (all eight libraries) |
| Debug path | `D:\Projects\DSTEAL02\Release\DSTEAL.exe` |
| Base / entry | 0x00010000 / 0x000EB57E |
| Kernel imports | 113 |

```
0x00011000 .text   926 KB   0x00128E20 DSOUND  38 KB
0x000F8760 D3D      70 KB   0x001324A0 WMVDEC  85 KB
0x0010A080 D3DX    115 KB   0x00147800 XPP     30 KB
0x00126CE0 XGRPH     8 KB   0x0014EF20 .rdata  24 KB (kernel thunks first)
0x00155000 .data   500 KB   0x001D1E20 DOLBY   27 KB
```

## What upstream's later branch found (sp00nznet/wreckless)

The repo was a suggested starting point; its main branch only boots, but the branch
`work/intro-video-and-attract-mode` went much further, on DSTEAL_JP.xbe and
the older toolkit (v0.13), on Windows:

- **Only the Japanese builds play the intro movie** (data\movie\LOGO1.wmv).
  default.xbe has no reference to it and no call into its WMVDEC section
  (the decoder is dead code there). [verified for default.xbe: the toolkit
  finds 7 functions in WMVDEC; "data\movie\LOGO1.wmv" is in DSTEAL_JP.xbe's
  strings and not in default.xbe's, whose only "WMV" strings are the
  section name.] So the "known blocker", the WMV stall, is not on the US
  path at all; it comes back only for a JP build (the disc has the movie).
- JP reached attract mode (replay10.dat, mapA01/st_demo.*), its pushbuffers
  decoded cleanly, but nothing executed them (no NV2A executor on that
  toolkit) and DirectSound was the gate: `sub_000E43E0` initialises the
  whole engine behind DirectSoundCreate, which failed without the AC'97
  ready bit, and with it hung on the DSP doorbell (3 written to
  [ebx+0x810], waits for 0). The takedawn runtime has both: AC97_READY and
  APU_DSP_ACK=auto. Their run used RECOMP_APU_DSP_ACK=0x80478810 on JP.
- Its 79 run-time seeds for default.xbe (config/seed_functions.json on that
  branch; `work/upstream_seeds_default.json` here): the fork's disassembler
  finds 59 by itself; the rest are in the cross-check below.
- D3D device pointer 0x0010A050 in default.xbe (0x001092E0 in JP), fence
  pair +0x30/+0x34, field count +0x245C: all three checked here (below).

## Generator, 2026-10-26

`./regen.sh` (the DS2 script on this toolkit only; WR_XBE= picks another
XBE into build/xr-NAME and recomp/gen-NAME). The whole pipeline takes about
35 s on this XBE (the fork's disassembler: 7 s; upstream v0.16's: 95 s).

First pass, no seeds: 4,159 functions, 75.1 % reachable, 4,166 translated,
0 failed, 20 unresolved call targets. Looking at those:

- **The immediate-reference pass made false function starts.** It takes
  any 32-bit immediate that lands in an unclaimed code gap and probes as a
  function as a function address. It runs before the bodies reached only
  through pointers exist, so their insides look like gaps:
  - 0x0007FFFF and 0x00080000 (the constants 524287 and 524288) landed on
    the "push ebp" and the "add ecx" in the middle of a 3.5 KB vtable
    method (0x0007F290), and its jumps to the shared epilogue at 0x00080141
    became an empty stub ("ret 12" without the pops: a wrecked stack).
  - 0x0001BA1A (113178) cut a callback found in the same pass
    (0x0001B990) after `mov al, bl` (8A C3: the C3 is not a ret), so two of
    its returns, 0x0001BA3D and 0x0001BA45, were stubs.
  Fix in the fork (tools/disasm/functions.py, `_starts_clean`): an
  immediate target must be a place only a branch can reach: a ret, an
  unconditional jmp or int3 before it, with only padding between, or a
  switch table's end. It removed 9 starts [verified each by hand]: 8 were
  false (inside code or a jump table); the ninth, 0x0010E9C3, is a real
  D3DX callback that follows a call to longjmp and a dead `pop esi`, so it
  is a hand seed now (the toolkit's own comments name it as a Wreckless
  alternate entry). Of general value; tell the Double-S.T.E.A.L 2 project.
- The rest were real functions the detector missed, reached by a jmp from a
  thunk or only through pointers (`mov ecx, [esp+4]; jmp X` thunks at
  0x000DD900..., 0x00013570...), plus the CRT x87 helper's second
  instruction (0x000F2CB2, the same shape as DS2's 0x0011E75A).
- Left as stubs, all dead [verified]: 0x000A300D (a `js` decoded from the
  switch table at 0x000A2FD4 after an unconditional jmp; the switch itself
  is lifted as a table), 0x0014EF4A/5A and 0xD4125481 (data decoded as
  code: the kernel thunk table, D3DX data).

### The seed cross-check

`config/seeds_crosscheck.json` (114, generated by tools/gen_seeds.py) holds
the function starts that one of three other sources finds and the fork does
not, kept to the safe ones:

- upstream xboxrecomp v0.16's disassembler on the same XBE (4,253
  functions; build/xr-up, ../xboxrecomp at f0f8a46);
- sp00nznet/wreckless's run-time ICALL seeds (above);
- Ghidra 12.1.4 through REA on tools/xbe2elf.py's ELF wrapper (2,844
  functions: it follows calls and not vtables, so its silence proves
  nothing, but what it finds is code).

Kept when tools/clean_start.py says the address is a clean start (only a
branch can arrive: the same test as the fork's fix), the sweep decoded an
instruction there, it is not a switch case, and the body holding it (if
any) never branches to it itself. 232 candidates: 114 kept; 109 were
reached by fall-through (upstream's own false starts, mostly
tail_jump_alias), 3 not decoded (switch tables), 3 internal branch targets,
2 switch cases (one of them upstream's run-time seed 0x001007BE, which the
fork lifts inside its switch). Ghidra confirms 19 of the kept, and none of
the dropped.

A seed at a clean start splits nothing that runs: the "fallthrough" into a
seed the lifter emits is after a ret and its padding [verified: every one
of the 28 such sites]. The regeneration added exactly the 115 seeded
functions and changed only the 41 bodies they split (tools/gendiff.py).
Result: 4,266 functions, 78.0 % reachable, 4,268 translated, 0 failed,
4 unresolved (the dead ones above).

Also found: the fork's alias entries keep the end of the body they were
made from even after a rebuild moves it (0x0009CD70's runs to 0x0009D4B0,
over two real functions); harmless once the functions inside are seeded,
not fixed.

## Runtime differences from DS2 (XDK 4134 against 5849)

- memmove/memcpy at 0x000F02C0 is the same CRT routine as DS2's 0x0011C1B0
  [verified: identical bytes to the first branch]; upstream main stopped on
  its tail arms ("[ICALL] Failed to resolve VA 0x000F0418"). Native, as in
  DS2 (recomp_native.c, config/manual_functions.json).
- The GPU fence: D3D's pushbuffer wait (0x000FE912) reads the submitted
  count at device+0x30 and the consumed pointer at device+0x34 [verified:
  disassembly], one dword later than 5849's. Device pointer 0x0010A050
  [verified: every D3D entry reads it, e.g. 0x000F8764].
- The field count: GetDisplayFieldStatus (sub_000FAF80) reads
  device+0x245C [verified], which the vblank interrupt moves
  (sub_001013D0 writes [miniport+0x1F4]; the miniport object is
  device+0x2268, set up by sub_000FFF20 -> sub_0010211D [verified]).
- **Software methods are encoded differently.** 5849 puts the method and
  its data in the NOP's parameter (data << 5 | method); 4134 sends the data
  first as SET_ZSTENCIL_CLEAR_VALUE (0x1D8C, PGRAPH 0x1A88) and, when there
  is a second word, SET_COLOR_CLEAR_VALUE (0x1D90, PGRAPH 0x186C), then the
  NOP with the method number alone [verified: D3D emits 0x00081D8C, two
  words, then 0x00040100 + 9 at 0x000F9536]. The PGRAPH service
  (sub_00101850) calls the dispatcher sub_001015B0 thiscall (method, data),
  ret 8, on the miniport object. Its methods [verified: the jump table at
  0x001017F4]: 1 swap now (sub_000FF5B0 writes PCRTC_START 0x600800 with
  the new front buffer, then the frame counts and the swap callback), 2 a
  swap at the next vblank, 3-4 sub_00101530, 5 KeSetEvent, 6-7 a callback
  with the second word, 8 an RDI write, 9 a register write. The swap being
  a software method means the display address comes from the title's own
  CRTC write, not from the flip registers.
  Fork change: xbox_Nv2aSetSoftwareMethodHandler4134(fn, device pointer,
  offset); the executor hands the dispatcher both clear values (and writes
  them to the PGRAPH register file) and runs method 1 too, and maps 1/2 to
  the pacing's immediate / one-vblank swap. [verified: runs dispatch 9, 2,
  8, 3 and 5 with the data the stream set, and the swap's PCRTC_START writes
  alternate between the two front buffers; a 60 s run made 2,880 flips,
  loading included.]
- Software method 3 is how this D3D runs a pushbuffer resource
  (sub_00101530): its data is a 4-word record written right after a CALL
  (fixup list, block base, exit offset, return address); it applies the
  fixups, points the block's exit JUMP back past the record, and writes
  NV_PFIFO_CACHE1_DMA_SUBROUTINE (0x324C) with bit 0 clear. So the block is
  CALLed but leaves by a JUMP, and the method clears "in a subroutine"
  before the next CALL [verified: the captured stream at 0x801E0A04:
  ZSTENCIL_CLEAR_VALUE = 0x801E0A18, NOP 3, CALL 0x83105978, the record,
  and the block's exit jumps to 0x801E0A28]. D3D's interrupt treats a
  pusher parse error as fatal (an int3 after "MP: GRAPHICS PARSE ERROR"),
  so the console never sees a CALL in a CALL. Fork change: the walker's
  subroutine state is now that register (kept across walks, published at
  call and return, read back after a software method). Before it, every
  demo frame stopped at the second CALL, the fence never reached D3D's wait
  for pushbuffer room (sub_000FE870, KeWaitForSingleObject on device+0x2440,
  which software method 5 sets) and the game hung on a black screen.
- **Vertex state programs.** Every model draw is preceded by
  SET_TRANSFORM_PROGRAM_START 81 (or 69), SET_TRANSFORM_DATA (1,0,0,1) and
  LAUNCH_TRANSFORM_PROGRAM: XDK RunVertexStateShader, a program that runs
  once and writes constant registers. The scene programs read their
  world-view-projection rows from c[22..25] [verified: program 18965A21's
  dph oPos, R0, c[22..25]; no SET_TRANSFORM_CONSTANT_LOAD in the stream
  ever targets 22; the dumped constants were zero]. Neither method was
  handled: every vertex came out at (0,0,0,0), and the demo was black on
  the Vulkan backend and the CPU rasteriser alike. Fork change: the
  executor runs the program on its CPU vertex interpreter with constant
  writes on. Found with the CPU path's RECOMP_TRACE=vsh (disassembly) and
  RECOMP_DEBUG=vsh_dump (program and constants), which only that path has.
- The gamma ramp: D3D writes SetGammaRamp's 256 RGB triples to the DAC
  through PRMDIO 0x6813C8/C9 from the vblank interrupt and the swap
  (sub_000FF634); that port is plain RAM in the runtime. A hook hands the
  ramp to the runtime (xbox_GammaSetRamp, new), which the Vulkan presenter
  applies as a post pass and the frame dumps apply too. The game loads the
  identity at start [verified: log]. It never sets another ramp: D3D's
  SetGammaRamp is not linked in; the only code that marks a ramp pending
  ([miniport+0x814]) is device creation (0x00101AD6, the identity from
  0x00102413), and the pending ramp is written at the next flip
  (0x00101350, from the vblank handler and method 2; method 1 at
  0x001015F6), both through the hooked writer [verified: disassembly of all
  code for +0x814/+0x81C]. So unlike DS2 (where SetGammaRamp's deferred
  write never ran and DS2 hooks SetGammaRamp too) nothing is missed here.
- maskmovq (the masked MMX store) was untranslated: the index-list builder
  at 0x0006B2D0 (silhouettes / shadow volumes: it tests faces with SSE and
  stores two or three 16-bit indices a store). Lifter fix in the fork.

## Bring-up runs, 2026-10-26

Headless, tools/run.sh, 40-90 s each (Play report: run freely unless a game is
being played; check the process list first).

1. Title screen at the first frame dump (logo, PRESS START BUTTON, the
   Activision legal text), then black once the attract demo started: the
   pusher stop above. 9,789 kernel calls fine, sound banks and BGM
   streaming, the DSP doorbell acked (auto), the pads up.
2. With the stop dump (DS2_PB_STOP_DUMP=file, the 64 MB contiguous window)
   the stream decoded offline (tools/pbdecode.py) showed method 3.
3. Demo runs to its end and back to the title, but black: all render
   targets of a demo frame (scene 512x384 A8R8G8B8, glow chain, the front
   buffer) were zero, with ~240 scene draws a frame. Ruled out on the way:
   the fog (colour FFEFBFB5, would be peach), stencil (ALWAYS), the depth
   clear (taken at the clear), the luminance readback (RECOMP_GPU_CPU_READS
   =16x16 changed nothing). The CPU rasteriser was black too, which put it
   in the shared state: the state programs.
4. Demo in 3D: the buggy, streets, signs, shadows, the game's motion blur
   and glow; the composite to the front buffer samples the scene three
   times with a per-channel offset and adds a 256x256 glow.

What the frame is made of (vk_draws at a demo flip): a 512x512 D16 shadow
map (59 draws, sampled by the scene through texture shader mode 2 with
SET_SHADOW_COMPARE_FUNC = GEQUAL), the scene into 512x384 (~240 draws,
fogged by the final combiner), a glow chain (256x256 down to 128x128,
added back at alpha ~0.67), the composite into the 640x480 front buffer
(letterboxed 11.5..467.5), 228 draws into a 256x256 target after it, two
chains down to 16x16 (luminance, as DS2's exposure; the readback did not
matter for the picture so far), and the HUD.

Open from these runs:
- The title is drawn at half intensity: its quad is the texture times a
  vertex colour of 0x7F with a plain MODULATE (no output shift), and the
  ramp is the identity, so the console computes the same [inferred from
  the GPU state]. Check against a capture of the real title screen.
  (2026-10-26: MobyGames' screenshots sit behind a bot check; still open,
  for a capture from a real console or a video.)
- One demo frame (f026 of the run) shows black blotches over a car: to
  look at (alpha-tested decals? the shadow compare? the glow?). Later
  (2026-10-26, render scale 2, run chk_demo f015): the same fence scene
  has only a small dark patch on an out-of-focus panel, and a whole demo
  dumped every 10 flips (run blot1, a different replay) has none. Only
  the buggy-at-the-fence replay shows it [guess: the depth-of-field or
  motion-blur pass sampling an unwritten part of a target]; parked.
- The 228 draws into 00AE4000 after the composite: what they are.
- Frame pacing not measured yet (2,880 flips in a 60 s run, loading
  included; DS2's game steps its logic per frame, likely this one too).
- Sound not heard (headless, dummy audio).

## Menus and the first mission, 2026-10-26

Driven with a live pad (RECOMP_PAD_LIVE: a line appended to a file is
pressed) and frame dumps. [verified: runs menus1, menus2, perf1]

- The title takes Start only while it is shown, about 15 s; then it loads
  the next attract demo with the title frame still on screen (the loading
  thread does not flip a new picture), so a press seen on a "title" frame
  can land during the load, which Start cancels: back to the title. That
  looked like a title ignoring Start for four presses in a row.
- The title has the same brightness in every dump (the mean of the frame
  is the same each time), so the half intensity is not a fade-in. The
  question stays open.
- Start opens NEW GAME / LOAD GAME / LOAD REPLAY; A on New Game gives the
  main menu (Mission, Replay, Stats, Options, Save, Exit), then Scenario
  select (A: Hong Kong police, B: the spies), Mission select, Car select
  (3D car turning; Patrol-1 and the buggy unlocked), Choose Mission Level
  (difficulty, traffic; A on the difficulty starts the mission).
- Mission A-1 loads (st_demo.mps/.tac, the briefing card), plays its
  opening cutscene with subtitles (Start skips it), then GO: a rolling
  start at 32 mph, HUD (time, speed, gear, target counter, minimap,
  damage). The right trigger is the throttle (59 mph in 4th), the left
  stick steers. Failing the mission (time out) gives "View Replay?".
- Probes for d:\data\mapA01\is_demo.mps, is_demo.tac, is_captmUS.txt
  fail: those exist only in the maps with an interim scene (is_ files in
  two maps); the game probes for them. Not a missing file.
- A scripted way in (times from the first pad read, generous):
  6000:start,9000:a,12000:a,15000:a,18000:a,23000:a,26000:left,28000:a
  reaches the cutscene of A-1 on Normal.

## Mission speed, 2026-10-26

Two runtime fixes (branch wreckless, now in antbox), each confirmed with
one run.

1. The mission ran at ~20 fps, slower the more there was to draw
   (RECOMP_TRACE=frametime: the executor busy 28 ms a frame on the street,
   88 ms in the park with its trees, of which only 3-9 ms were draws).
   Sampled with gdb as the game's parent (tools/gdbprof.py; ptrace scope 1
   allows no attaching): 74% of the executor was in the title's software
   method dispatcher (sub_001015B0), 48% spinning in sub_000FE6A0 [verified:
   work/runs/prof3/prof.txt]. Methods 3 and 4 run a recorded pushbuffer
   (copy its fixups, write the jump back, set DMA_SUBROUTINE for 3) and end
   with sub_000FE6A0: set PFB 0x100410 bit 16 (flush the write-combine
   buffers) and spin until it clears. One for each mesh, so 100+ a frame.
   The dispatcher runs on the executor thread, so a helper thread clears
   the bit; it napped 1 ms between methods and ~70 us during one, and every
   method waited out a nap. Now it polls without sleeping during a method,
   keeps polling 200 us after one, and otherwise sleeps on an event the
   next method's start sets (xbox_memory_layout.c, swm_kick_thread). The
   executor went to ~9 ms a frame [verified: run swm1].
2. Then the mission ran at 60 with its clock at double speed (28 game
   seconds in 14 of wall time [verified: dump times against the HUD
   clock]). The game's frame wait (sub_000DC5E0) spins until the vblank
   count, device+0x245C (the miniport's +0x1F4), has moved by two: 30 fps
   on the console, the logic stepped for 30. The D3D vblank handler moves
   it once a vblank, and the runtime's frame counter (registered in
   main.c, from upstream's bring-up when no vblank ran) added one more at
   every flip, so a frame got through every vblank. The counter now stands
   down the first time it finds a value it did not leave there ("Frame
   counter: device +0x245C moved by the title"), which is what main.c's
   comment already said it did. The mission now runs at a steady 30 fps
   (33.3 ms frames) with the clock in step with the wall [verified: run fc1,
   12 game s per 12 s, 11 per 11, ...]. The 20 fps runs above were at 2/3
   speed (13.6 game s in 20): the logic steps 1/30 s a frame however long
   the frame took, so below 30 fps the game slows down [guess: fits both
   measurements; not read in the code].

Left: two stretches of ~9 s with 330-370 ms frames in run fc1, about a
minute apart, the draws per frame unchanged. Other sessions had the
machine loaded (load average 15, a LA Rush run and two Python jobs, 29 GB
in swap) [guess: that is the cause]; to measure again on a quiet machine.
Run iv2 (same build, same hour) stood 22 s without a flip on the mission
level screen and went on by itself; its hang report has no thread
answering the sampler at all, as if the process was not scheduled [guess:
the same contention].

## Sound, 2026-10-26

Recorded headless (RECOMP_AUDIO_WAV, run snd1: title, menus, mission A-1,
replay): sound all the way, RMS 1,000-6,000 a second, peaks to 30,934 of
32,767, the channels apart where 3D voices pan [verified: per-second RMS
of work/snd1.wav]. The audio clock keeps time: 10.4 s written per 10 s of
vblanks. By ear still to do (a window run). DSOUND 4134 through the APU
needed nothing new.

The run went on 60 s past its stop because a script started it with `&`:
a non-interactive shell's background jobs ignore SIGINT, the runtime only
takes over signals left at their default, and the watchdog ended it.
tools/run.sh now stops with SIGTERM. Not a game fault.

## Saves, 2026-10-26

Work end to end [verified: runs save9, load1]. Main menu > Save: the
screen lists the hard disk (FreeBlocks 50,000+) and nine empty memory
unit slots. A on the hard disk moves into its slot list; A on "Empty
Space" writes U:\02D8748D8733\ (SaveMeta.xbx, SaveImage.xbx, game.xsv),
shown as "MIS A1 10/26/26", the date and local time, and a picture of the
car. After a restart, Title > Load Game lists it; A, A reads game.xsv and
opens the main menu. On the host: work/save/UDATA/4156000a/.

On the way: the slot list (sub_000AB0C0) looks like the device row with
only "Empty Space" lit differently, so B or a direction pressed there
looked like a screen that had stopped taking input. Before A on the slot
it is waiting for A. Tools left from that: work/gdbpeek.py (guest fields
at g_esi when the game thread is in a given function) and work/gdbbrk.py
(break on generated functions, print arguments and g_eax on return).

## Video modes, 2026-10-26

The game has no 480p, 720p or widescreen mode [verified: run vm1 with
display.hdtv=720p and display.widescreen=1 (XC_VIDEO=0x000B0000): it
asks the kernel for settings 0x11, 0x0A and 0x09 only, never 0x08
(XC_VIDEO, where XGetVideoFlags reads 480p and 16:9), and makes 640x480
front buffers as before]. The runtime's display.aspect is 4:3 only for
now. Widescreen would take a patch of this game (the camera's aspect and
field of view, the HUD's placement), as Burnout 3 has; a later item.

Defaults now as DS2's where they apply (config/enhance.toml): render.scale
2, graphics.aniso 16, filter linear. The mission holds 30 fps with them,
the executor 4-10 ms a frame [verified: run rs2, machine loaded by other
sessions at the time].

## Correctness sweep, 2026-10-26

Headless, live pad [verified: run sweep1]: Options (vibration, music,
effects and engine volume, music test; no controller layout, so the
runtime's settings row would go here), Game Stats, Replay mode (View and
Save greyed until something is played, Load offered), Scenario B and its
mission B-1 (dusk cutscene with subtitles, READY?, the spies' car with
the damage meter, "Wrong way!") all as expected.

## Settings overlay, 2026-10-26

On (CMake XBOXRECOMP_OVERLAY, like DS2): Back + Start or F1 opens the
runtime's settings in a window; what the player changes goes to
settings.toml in the save folder, and enhance.toml stays the defaults
(src/game/settings.c, DS2's shape). No rows of the game's own yet; hidden:
aspect ratio, UI scale, the texture upscalers, overscan crop, button
glyphs and the in-game menu entry (OPTIONS has no row for it yet). The
overlay's C++ needs -DCMAKE_CXX_FLAGS="-isystem <sysroot>/include/
x86_64-linux-gnu" like the C flags (README). Headless runs load the
settings ("no file yet") and play as before [verified: run ovl1]; the
overlay itself draws only in a window, so it waits for a window run.

## ABI check, 2026-10-26

A build with -DRECOMP_ABI_CHECK (build/linux-abi, binary work/bin-abi,
overlay off) through a demo cycle, the menus and mission A-1 (run abi1,
tools/mission_drive.sh with BIN= and DELAY=46000) flagged five functions,
all CRT helpers with their own conventions [verified: disassembly]:
__SEH_prolog 0x000F0954 and __SEH_epilog 0x000F098D (move esp, restore
ebx/esi/edi from the frame), the stack probe 0x000F0020 (moves esp), a
64-bit divide 0x000F5A70 (remainder in ebx), an x87 helper 0x000F2C13
(ebx). No game function returns with a callee-saved register changed.

## Packaging and the Deck build, 2026-10-26

As DS2's: ./deck-build.sh compiles the same generated code in an Ubuntu
24.04 container (podman, container/Containerfile = DS2's, so it reuses the
image localhost/ds2-build:24.04; no download) into build/deck; the binary
needs GLIBC_2.38 and GLIBCXX_3.4.32, so it runs on SteamOS [verified:
objdump]. It plays the title and the demo headless on the laptop [verified:
run deck1]. ./package.sh [DEST] makes the folder (binary, enhance.toml
unless DEST has one, README, licenses, VERSION with both trees' commits);
WR_BIN=build/deck/bin/wreckless ./package.sh dist/wreckless-deck is the
Deck package (42 MB).

## The picture, the pause menu, 2026-10-26 (the first play in a window)

First play in a window: "game is extremely blurry, menus are too dark, and
pause menu options are blank". Reference: World of Longplays' Xbox
longplay of the game (YouTube qVFlzdsHdLM), measured in the browser by
drawing the paused video into a canvas (no download).

- 3D too dark, and blurrier: the CPU reads the two 16x16 targets at the
  ends of the 64-32-16 chains (exposure, as DS2's) and they were never
  written back. RECOMP_GPU_CPU_READS=16x16 is now a default (main.c): a
  mission's mean went 36 -> 64, and its edges up by a tenth [verified:
  runs chk_mis, rb1]; the capture's driving frames measure 51-71.
- Everything darker than the capture in the midtones by a curve: our
  frames to the power 0.8 match it within a few levels on the title, the
  car select and A-1's READY? street (numbers in picture.c). Now the
  default "TV picture" (src/game/picture.c, on top of the game's gamma
  ramp through the runtime's gamma pass; settings row "TV picture",
  game.tv_picture, RECOMP_TV_PICTURE=0 for the raw output). After both,
  the car select measures 25/69/97 against the capture's 39/67/102 (its
  black lifted) [verified: run pause4].
- Pause menu blank: its items are font quads (prim QUADS, the 256x512
  DXT5 font) whose position has z = 0 and rhw = 0; the program's
  reciprocal makes w infinite, and the Vulkan screen-space epilogue kept
  it, so every coordinate went infinite and nothing was drawn. The NV2A
  clamps w to +-2^64. A one-line clamp in the toolkit
  (src/d3d/d3d8_vsh_glsl.c) draws them [verified: run pause4: PAUSE,
  Retire, Restart, Music/SE/Engine, Return]. Sent to antbox's maintainer as
  4c57115 (a local toolkit branch, on 3cb29ed,
  not pushed); ANTBOX_PIN is 4c57115 until antbox has it.
- Second window run: "menus are too dark and game is completely blurry";
  Play report: "It is 100% a depth of field issue", and a real Xbox's menus are
  that dark too, so the TV picture is now off by default (the row stays).
- The blur was the depth of field reading the wrong kind of depth. The
  game draws its 3D W-buffered: SET_CONTROL0 = 0x110001, bit 16
  Z_PERSPECTIVE_ENABLE [verified: gdb read of s_vsh.control0 in A-1]. Its
  DOF pass (on the 512x384 scene before the composite) reads the scene's
  Z24S8 depth as LIN_A8R8G8B8, looks the top byte up in a 256x1 A8 ramp
  (0x8046D000: 00 00 00 00 01 01 02 03 03 04 ... [verified: gdb read]) by
  DEPENDENT_AR, and blends a blurred copy by it. On the console the
  W-buffer's depth is w over the clip range, linear in distance; the
  Vulkan backend ignored the bit and stored z/w, so nearly every pixel
  read as far and took the strongest blur. Toolkit fix: the depth is
  w * (1/clip_max) when the bit is set (5430d4a on branch wreckless-wbuf
  in a local toolkit branch, on antbox 6f1eb62; sent to antbox's maintainer).
  Mission frames: edge measure 7.5 -> 10.3, sharp, no sorting faults
  [verified: run dofw1]. The depth is per vertex (xemu's is per pixel).
- The composite itself also averages three samples of the scene half a
  texel to a texel apart (combiner thirds) and adds the glow: a soft look
  by design, kept.


## The A-2 freeze, 2026-10-26 (the third play session)

In mission A-2 the game stopped drawing (no flip for 45 s, hang report
~/.local/share/wreckless/crash/20261026-201748-hang). The kernel-timer
thread held the IRQL gate in DirectSound's DPC (sub_0012C1EF -> ... ->
sub_0012DB98) and spun in sub_0012CD75, which waits while bit 15 of a
voice's flags word (+0x12) is set; the game thread (XInput, raising IRQL)
and the executor (a software method entering interrupt level) waited for
the gate. Bit 15 is set by sub_0012D6DC and cleared by the APU interrupt
service (vector 5, routine 0x0012C3D9 -> ... -> sub_0012CB15) [verified:
KeConnectInterrupt log, disassembly of every DSOUND function writing
+0x12]. On the console that interrupt preempts the DPC; here an interrupt
runs on the gate holder at a safe point, and the tight loop had none. A
hook (recomp_hooks.c, sub_0012CD75, a full replacement) runs
xbox_IrqSafePoint on each pass. That did not end it: the next play session
froze the same way on Retire, see below. A general note for the toolkit:
any DPC that spins on what an ISR sets has the same shape.

## The Retire freeze, 2026-10-26 (the fourth play session)

Retiring from a mission froze (hang report 20261026-204436-hang); so had
the end of A-2 twice (-201748, -202648), with the same kernel-timer stack
in all three: sub_0012C1EF (the DPC) -> sub_0012C125 -> sub_0012BCDF ->
sub_0012F1C1 (a stream voice's service) -> sub_0012F113 -> sub_0012EE6B
(submit queued packets) -> sub_0012EC76 -> sub_0012DB98 (play) ->
sub_0012CD75, spinning on bit 15 of voice 0x01C1D574 [verified: the
reports]. In -204436 the stream's worker thread (guest-w-8A42) spins on
the same voice in sub_0012F033, the stream flush: it raises IRQL, stops
the voice (sub_0012D6DC sets bit 15 and writes VOICE_OFF), lowers IRQL,
then waits for bit 15 before aborting the queued packets [verified:
disassembly].

Why nothing clears bit 15: sub_0012BCDF raises a guard on the APU object
([obj+0x2C0]++ at 0x0012BCEA, -- at 0x0012BD2D) around its pass over the
voice lists at obj+0x6C4, and the interrupt's idle-voice handler
sub_0012C177 returns without doing anything while that guard is up (cmp at
0x0012C182). The trap is consumed (sub_0012C302 resumes the front end
regardless). The VP traps the idle voice again every frame, and every
trap is dropped while the DPC spins under the guard [verified: guard = 1
in -204436's guest RAM; disassembly of sub_0012C216/C302/C177]. Only
sub_0012CB15 (via sub_0012BE15, from the trap) clears bit 15
[verified: every write of +0x12 in DSOUND, listing work/dsound_dis.txt
made from the DSOUND section, VA 0x128E20; the older work/all_dis.txt is
misaligned there].

On the console it is a race DSOUND nearly always wins: the VP traps the
off within a 0.67 ms frame, the handler unlinks the voice from the lists
(sub_0012CB15), and a DPC that runs after that never sees the voice. A
DPC has to be pending at the very moment the flush lowers IRQL and find a
free SSL slot on that stream. Here the trap comes later (the frame thread
renders 5.33 ms periods; a trap while the gate is held is posted to the
holder) and DPCs queued during the flush's DISPATCH stretch run the
instant it ends, so the window is wide.

Fix (recomp_hooks.c, sub_0012EE6B, a wrapper): when the voice has a stop
pending and the guard is up, the submit is skipped (returns 0); the
packets stay queued and the flush aborts them after the stop lands. That
is the state that otherwise spins forever, and it is what the DPC would
see on the console once the trap had unlinked the voice. The log says
"[DSOUND] stream ...: submit held" when it steps in. [guess: it ends the
Retire and A-2-end freezes; one headless retire never hit the race.]
Toolkit-wide, the closer fix would be to deliver a pending idle trap
before DPCs run when a thread drops out of DISPATCH (sent to the antbox
maintainer as a note, not a change).

## Objects in the wrong order, 2026-10-26 (the fourth play session)

The player saw "a few objects rendered in the wrong order" in scenario A. With
W-buffering (CONTROL0 bit 16, 5430d4a) the vertex stage gave each corner
w over the clip range and the rasteriser interpolated it linearly across
the screen; inside a big triangle that is the corners' arithmetic mean of
w, deeper than the pixel's true w (the harmonic mean, which the NV2A gets
by interpolating 1/w), so whatever stands behind a large road or wall
triangle can draw through it. Toolkit change 6b75477 (local branch
wreckless-wdepth): the vertex stage outputs that depth as a varying
(o_wz), interpolated perspective-correct, and W-buffered combiner shaders
write it as gl_FragDepth. Checks: chk_demo and chk_mis pass; A-1 frame
times 33.4 ms mean / 33.6 ms p99 against 33.7 / 33.6 before (30 fps cap,
so the lost early depth test costs nothing visible here). [guess: it is
what the player saw; the runs are not frame-synced, so no before/after of a bad
spot. Needs a player's eyes.] Not covered: fs_basic draws, the slope part of
polygon offset on W draws.

## The scenario B opening, 2026-10-26

Play report: the opening cutscene before scenario B doesn't play. B-1's opening is
st_demo with the 76 s voice track sound\demoUS\B1opdemo.wav and captions
from st_captmUS.txt (a frame timeline). Headless with no input after the
level screen it plays all the way (run scenb2: 37 dumps of 60 flips from
the voice track's open to the mission music, every cut and caption). A
press of A once loading has started skips it (run scenb1, a stray
scripted A); Start skips A-1's. In both window sessions the
mission music (BT23.wav) followed the voice track within the same 10 s,
and A-1's opening ended as fast in the second, so something skipped them
there. Not a resting input: the player's pad (Hyperkin Duke, 2e24:0b22, "Generic
X-Box pad") reads no button or trigger at rest, sticks under 6%, and the
game's pad code (around 0x000E2289, after XInputGetState 0x00149A5E)
uses a 0.3 stick dead zone and analog buttons > 30 [verified: SDL and
evdev reads, disassembly]. Open: did the player press anything during loading?
The is_demo.tac the game asks for is not on the disc; many maps have no
.tac, so that is normal.

## PC SETTINGS in OPTIONS, 2026-10-26

The game's OPTIONS screen has a PC SETTINGS row under MUSIC TEST that opens
the settings overlay, as DS2's PC SETTING does (src/game/options_row.c,
whose header has the layout format; hooks in recomp_hooks.c).
- The screen: front-end state 13 "Option" (state table 0x00157248, 0x14
  a state: screen, name, setup, frame, leave), screen file 9
  data\Screen\optionUS.xpr. Setup sub_000DD1C0 (row 0, the volumes and
  two flags from the sound and pad objects), frame sub_000DDDC0, leave
  sub_000DD9C0 (applies [+0x81] vibration and [+0x82]). The rows are code:
  [front end +0x70] 0 VIBRATION, 1 CONTROLLER-TYPE (pad or wheel, skipped
  unless the language is 2; its art is in option_moji.tga but not in the
  US layout), 2-4 the volumes, 5 MUSIC TEST, 6 EXIT. Up and down (menu
  event 0x001CD50C = 0x0B, 0x0C; left 0x0D, right 0x0E, A 1 or 2, B 3 or
  4) move it and select the layout item 10 x row + choice
  (sub_000A6F40). The front end's frame (sub_000DE940, state 13 at
  0x000DECEB) leaves on A at row 6 [verified: disassembly].
- The layout is DS2's format, older: 4 bytes shorter header, 32-byte
  elements with an item and a parent and no links, no animations. The
  item a cursor selects needs element flag 0x20; with 0x40 too (EXIT, the
  values) it is dimmed until selected, then pulses. PC SETTINGS has both,
  item 70, at (130, 288); EXIT moved from y 290 to 322.
- sub_000A68A0 (relocates a layout in place) gets the copy instead, built
  in rows 96-99 of option_moji.tga, empty; the label goes into the empty
  right half of the VIBRATION cell, cut from the screen's own letters at
  run time (P of CONTROLLER-TYPE, C, S and E of SE VOLUME, T of MUSIC
  TEST, I N G of ENGINE VOLUME, S of MUSIC; work/opt/label.py is the same
  cut in Python). Checks: the layout's shape and names, EXIT where
  expected, the source cells' showing pixels (11123 +- 60, the US labels)
  and the destination rows empty; anything else leaves the screen alone.
- sub_000DDDC0's wrapper moves the cursor to, from and past row 7 (PC
  SETTINGS) with the screen's own move sound, and on A plays EXIT's confirm
  and asks the overlay to open (xbox_OverlayRequestOpen).
- The row shows only when the overlay can open (a window; not headless) and
  interface.menu_entry is on. RECOMP_MENU_ENTRY=force shows it headless,
  for a check (A then logs that there is no overlay). Runs opt3, opt4: the
  row drawn, every move to and from it, EXIT still leaves.

## Overhead, 2026-10-28

ROADMAP 5b (the project's "remove the overhead" goal); the lead came from the antbox
maintainer, out of Double-S.T.E.A.L 2's work (its NOTES.md "Overhead").
- D3D's KickOff (sub_000FE6D0) sets PFB 0x100410 bit 16 and spins until the
  GPU side clears it, `test [eax+0x100410], 0x10000 / jne` at 0x000FE710
  [verified: disassembly], as DS2's 0x0012FAB0. It is now listed in
  config/spin_waits.json, which regen.sh passes as --spin-waits: the back
  edge calls RECOMP_SPIN_WAIT, which in pacing "sleep" blocks until the
  runtime's xbox_SpinWake (the ack's kick_cleared) or 1 ms, and never at
  DISPATCH or with the one-CPU gate held. The generated code differs in that
  one line. The flush's own spin in sub_000FE6A0 (0x000FE6C0) is left: it
  runs on the executor too, once a mesh, where a 1 ms nap once cost 10 fps
  (Mission speed).
- The lowered wait sleeps only under pacing "sleep", so config/enhance.toml
  names present.pacing = "sleep", spin_lead = 8 (Balanced), as DS2 does.
- A/B on the Win Max (work/ab/ab.sh: A-1's scripted drive, 110 s a run,
  interleaved; per-thread CPU from /proc over 40 s of the mission, frames
  after 40 s; the machine at load 5-7 from other sessions) [measured]:

| run | code | pacing | process | guest-main | nv2a-ack | guest D3D a frame | frames mean / p99 / max | > 40 ms |
|---|---|---|---|---|---|---|---|---|
| ab_old_spin | as before | spin | 144% | 97% | 20% | 23.2 ms | 33.37 / 33.52 / 116.8 | 1 |
| ab_new_sleep | KickOff lowered | sleep, lead 8 | 116% | 43% | 45% | 5.2 ms | 33.37 / 33.35 / 100.0 | 1 |
| ab_old_sleep | as before | sleep, lead 8 | 180% | 96% | 51% | 22.4 ms | 33.37 / 33.37 / 133.3 | 1 |
| ab_new_sleep2 | KickOff lowered | sleep, lead 8 | 124% | 43% | 48% | 4.8 ms | 33.37 / 33.36 / 116.7 | 2 |

  The single long frames are in every run, the old build's too: the load.
  Balanced alone costs more (its 8 ms spin lead on the ack thread while the
  main thread still spins); with the wait lowered, a quarter of a core less
  than before and the main thread from 97% to 43%. What it still does is
  work: game 6.6, D3D 5.0, XDK 1.4, host 1.1 ms a frame (frametime's g_*),
  not spinning. No new error lines; "moved by the title" in every run.
- The attract demo has two outcomes from about flip 960 on, on the same
  binary (2026-10-30: two pin runs 9 of 16 dumps identical; other pairs
  all identical) [cause not looked into]. A single old-vs-new demo diff
  proves nothing; run each side twice.
- Not here: MA2's per-frame DSP reset (2026-10-30, antbox's maintainer's lead).
  This DirectSound has no compare with 0xCCCCCC (the bytes occur once, in
  data at 0x001D1E38). Its reset, sub_0012B9CE (GP and EP, then a 10 ms
  KeStallExecutionProcessor), ran once in a run, at start, and its recovery
  caller sub_0012BD65 never [verified: gdb counts, 20 s from boot; 90 s
  through mission A-1 saw no stall of any length after the first 2.5 s, at
  33.41 ms frames].
- Next candidates: the ack thread's 45% under Balanced (the spin lead, by
  design; Battery saver is lead 0 on the Power page), the executor's flush
  waits, D3D's 5 ms of push-buffer building.

## The PC SETTINGS page, 2026-10-27

Play report: the toolkit's overlay looks like Burnout 3's menus, not this game's;
and the game needs an exit (both: an EXIT on the page and a way to quit).
PC SETTINGS now opens a page of the OPTIONS screen itself
(src/game/pc_page.c; its elements in options_row.c's layout copy).
- Rows: WIDESCREEN, RESOLUTION, TEXTURE FILTER, ANTI-ALIASING, BRIGHTER
  MENUS, TV PICTURE, FULLSCREEN (left / right change, A steps round; through
  xbox_SettingSet, so live where the toolkit can be and kept in
  settings.toml when the page closes; a change that waits for a restart says
  NEXT START past its value), MORE SETTINGS (the overlay, for the rest),
  QUIT GAME (A asks "QUIT GAME? PRESS A AGAIN", A again quits as the
  overlay's Quit game does: recomp_quit_request, then SDL_QUIT, the window's
  close), EXIT (and B): back to OPTIONS on PC SETTINGS.
- The screen's element objects keep their place from when the screen is
  built: x, y floats at +0x0C, +0x10, colour +0x14, a text's string at
  +0x24 (up to 128, sub_000A7680) [verified: runs exp1, exp2]. The page's
  elements are built 4096 to the right; opening moves them back and the
  OPTIONS rows away (and back on close).
- Input: the front end's frame sub_000DE940 runs the cursor's input, then
  the state's own handling (OPTIONS' B and EXIT's A), then the state's frame
  function last [verified: disassembly], so the page takes the event in a
  wrapper of sub_000DE940 before any of that and clears it.
- Words: the layout's text elements draw with a small font for numbers
  (digits and a handful of letters; style 0x10 or 0x80 alike) [verified:
  runs page1, page2: boxes]. The page draws its words with the game's
  CXBFont (sub_000E1C40(x, y, colour, char *, flags) on the font at
  0x001A6CA8, Font18.xpr, 32-255; the colour through sub_0001FB00 as the
  game's "REPLAY"/"DEMO" captions do) after the menu loop flushes the
  screen's sprite batch (sub_0001E0D0 on 0x00173FC0, before the present).
  The text elements stay, empty, as the cursor's places, so the screen's
  own arrows show on the selected value. Labels green, values yellow, the
  cursor's row brighter and its value pulsing.
- Checks (headless, RECOMP_MENU_ENTRY=force): runs page3, page4: every row
  drawn, left/right/A changes kept in settings.toml, NEXT START on a
  resolution change, QUIT GAME's second A logs "[QUIT] asked for", B back to
  OPTIONS (page1).
- QUIT GAME didn't quit at first (2026-10-29): recomp_quit_request only
  logs on a desktop, its hook being Android's; the overlay posts SDL_QUIT
  itself [verified: recomp_restart.c, overlay_ui.cpp quit_now; the
  pre-fix build ran on to its 60 s timeout]. Now the page posts it too.
  Window check (work/nvr/win_ovl.sh: the pad script into the page, the
  overlay's test file for shots and Esc): MORE SETTINGS opens the overlay,
  Esc closes it, QUIT GAME's second A ends the game, exit 0, within a second.

## Widescreen, 2026-10-26

game.widescreen (on by default; overlay Display > Widescreen, live):
missions, replays and cutscenes 16:9, the menus 4:3 (src/game/widescreen.c,
hooks in recomp_hooks.c, toolkit nv2a_hud_squeeze).
- The camera: sub_000213C0 gives the camera its aspect pair for the view
  mode at 0x0016D860: mode 0 (one view, 0x00021400's (0, 0.025)-(1, 0.975))
  (0.75, 1); mode 1 (the chase scenes' two panels, (0, 0)-(2/3, 1/2) and
  (1/3, 1/2)-(1, 1), 16:9 each) (0.5625, 4/3); mode 3 side by side. Its two
  callers (0x00029C01, 0x000307B6) build the view from it [verified:
  disassembly]. With the setting every pair is scaled for a picture shown
  4/3 wider than the buffer: height/width x 3/4, width factor x 4/3 (mode 0
  then gets the panels' own pair). The frame stays 640x480, drawn
  anamorphic as Xbox widescreen was; Hor+ (the same height of view). No
  pop-in at the sides seen in runs wide3-wide8.
- The window: nv2a_present_frame_aspect_fn says 16:9 for frames while the
  mission loop runs (sub_000DB330, as picture.c's menu brightness) and the
  configured 4:3 in the menus; the window opens 16:9 with the setting on.
- The HUD: drawn into the 640x480 frame by one vertex program, hash
  0F208BAB, positions a fraction of the screen's width, a handful of draws
  a frame (the timer and score batch, the meter, the map and its markers,
  the car icons, tags over cars, the pause menu, a full-screen flash)
  [verified: RECOMP_TRACE=vk_draws, runs wide2, wide7]. The toolkit's new
  nv2a_hud_squeeze(hash, 0.75) narrows each element of those draws about
  its own middle in the Vulkan backend: elements are the triangles sharing
  corners, then small pieces within 2% across / 1% down (words, a tag and
  its bar, the gauge); a piece over 30% of the width (a caption's box)
  stays alone, one over 90% (the flash) untouched. Squeezing per draw
  instead pulled the car icons to the middle on the READY? frames, where
  they shared a draw with the caption's box (run wide4: 502..530 px where
  they belong at 567..594). The "hud element" lines of a vk_draws trace
  list the elements found.
- Cost: none measurable [verified: work/time_wide.sh, 2026-10-27, load
  1.7, A-1 scripted drive, 110 s each, frames after 40 s]: 4:3 33.39 ms
  mean / 33.61 p99, executor 12.0 ms, process CPU 50.9 ms a frame;
  widescreen 33.40 / 33.60, executor 10.5 ms, CPU 49.4 ms (the HUD
  squeezed in it: the map 107 px wide against 143). The 24-30 ms of
  runs wide4 and wide8 were the machine at load 16.

## Brighter menus, 2026-10-26

Play report: "we should still slightly bump up brightness of main menus". The menus
alone now get a curve (gamma 0.7, picture.c), switched by hooks on the menu
loop sub_000DF100 and the mission loop sub_000DB330, which never run in the
same stretch [verified: gdb samples]. Setting "Brighter menus"
(game.menu_brightness, on by default). Main menu 21/82/115 and car select
30/82/110, against 12/50/82 before; mission frames unchanged [verified: run
chk_mis].

## Status, 2026-10-26

Repo set up on the DS2 base; generated (4,268 functions, 4 dead stubs);
built (bin/wreckless); headless: title, attract demo in 3D, every menu to
mission A-1, which plays (cutscene, driving, HUD, fail, replay) at a
steady 30 fps in real time; sound comes out; saves write and load. Not
tried yet: a window, sound by ear, a mission finished.

## Controllers, 2026-10-31

The rule: any connected controller drives player 1, in any order, plugged in
before or after the start, and after one is unplugged. tools/pad_test.sh checks
it in a real window with virtual controllers (tools/vpad.py, /dev/uinput), run
with SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1 so it doesn't need the window's
focus. This game failed it at first, in the toolkit's port code (xinput_device.c: a
controller takes the lowest free port at its first press; ohci.c: one emulated
pad unless RECOMP_USB_PADS says more) [verified: run pad_test, its [INPUT]
lines and shots; reported to antbox's maintainer]:
- A controller plugged in late works as player 1 if it presses first.
- A second controller takes port 1, which nothing reads: its presses do nothing.
- Unplug the player 1 controller and the other stays on port 1: still nothing,
  until it is unplugged and plugged back in.
- A controller's first press on a new port is lost when short: the port isn't
  read again for up to 1 s (XBOX_INPUT_PROBE_MS). Three taps of Start: nothing,
  the title's menu, New Game.
Antbox 96958b0 fixes it [verified: pad_test, every step of the
cursor as predicted; a first tap works; a pad resting off-centre holds nothing;
rumble reached two idle virtual pads, 86 plays each at full, in mission A-1, with
the toolkit's background mute forced off under gdb, since it mutes an unfocused
window]. In use here.
Not shown by this test: the keyboard. Its pushed key goes down and up at once,
too short to read; the merge on port 0 is by reading input_core.c only.
B is reverse [verified: play, 2026-10-31]. The other buttons' meanings (README) come
from Shinya Aoki's 2002 FAQ for the US Xbox version, the only source found; it has no controls section, they're in passing,
and it says they can't be changed. PS2's layout differs. A scripted run through
every button showed nothing clear, the car having wedged in trees.
