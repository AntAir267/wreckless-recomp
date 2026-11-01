# Wreckless: The Yakuza Missions — Static Recompilation

A static recompilation of the original Xbox game **Wreckless: The Yakuza
Missions** (Bunkasha / Activision, 2002; internal codename **DSTEAL**, the
first *Double-S.T.E.A.L*) into a native Linux program. The game's x86 code is
translated to C and compiled for x86-64. The Xbox kernel is replaced by native
code. The graphics chip, sound chip and USB pads that the game's own libraries
talk to are still simulated in software, the graphics through Vulkan.

It is built on the work of the **Double-S.T.E.A.L: The Second Clash** recomp
(the sequel, same studio): its layout, scripts and runtime. The runtime is
[antbox](https://github.com/AntAir267/antbox), a shared Xbox toolkit built on
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp) and its
[`takedawn` branch](https://github.com/xenophobentx/xboxrecomp/tree/takedawn).
The earlier public attempt,
[sp00nznet/wreckless](https://github.com/sp00nznet/wreckless), is where the
layout first came from; its later branch's findings are in NOTES.md.

> You must provide your own copy of the game. No game code or data is
> included here.

**Status: early, playable.** See [Status](#status), [ROADMAP.md](ROADMAP.md) and
[NOTES.md](NOTES.md).

## XBE

`default.xbe` on the disc is the US game (byte for byte `DSTEAL_US.xbe`).

| Property | Value |
|----------|-------|
| Title | WRECKLESS, 0x4156000A, North America |
| Built | 2002-01-08, XDK 1.0.4134 |
| Debug path | `D:\Projects\DSTEAL02\Release\DSTEAL.exe` |
| Base / entry | 0x00010000 / 0x000EB57E |
| Kernel imports | 113 |

## Building (Linux)

Needs Python 3.10+ with `capstone` (in `.venv`), CMake, Ninja, Clang 20 and
lld, and SDL2, libepoxy, OpenSSL, ffmpeg, Vulkan headers, glslang and
SPIRV-Tools development files.

```bash
git clone https://github.com/AntAir267/antbox ../antbox
git -C ../antbox checkout "$(cat ANTBOX_PIN)"
python3 -m venv .venv && .venv/bin/pip install capstone
./regen.sh                       # default.xbe -> src/game/recomp/gen/*.c
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 \
      -DCMAKE_EXE_LINKER_FLAGS=--ld-path=/usr/lib/llvm-20/bin/ld.lld
ninja -C build/linux
bin/wreckless [--headless] [--save-dir DIR]
```

The game files go in `game/Wreckless - The Yakuza Missions/` (extracted from
your disc image; 1.7 GB), before `./regen.sh`.

The host's build needs the host's glibc. For the Steam Deck (SteamOS) and
other older systems, `./deck-build.sh` compiles the same generated code
inside an Ubuntu 24.04 container (podman; `container/Containerfile`) into
`build/deck`.
`./package.sh [DEST]` makes a folder to run from (the binary, the default
settings, this README, the licenses and a VERSION file);
`WR_BIN=build/deck/bin/wreckless ./package.sh DEST` packages the Deck build.

## Playing

You need your own disc: the Redump image of *Wreckless - The Yakuza
Missions (USA)*, extracted into `game/Wreckless - The Yakuza Missions/`
(or anywhere, with `--game-dir`). Saves and the emulated hard disk go to
`~/.local/share/wreckless` unless `--save-dir` names another folder.

The first start copies the menu screens and some mission files to the
emulated hard disk's cache, as the console does.

### Controls

| Pad | In the game | Keyboard |
|---|---|---|
| Right trigger or A | Accelerate (A is also OK in menus) | W or Up; Space for A |
| Left trigger or X | Brake | S or Down; Shift for X |
| Left stick | Steer | A, D or Left, Right |
| Right stick | Look around | |
| White | Zoom the view | Q |
| Y | The action where one is offered (elevators, the Tank-90's cannon) | Ctrl |
| B | Reverse; back in menus | Backspace |
| Start | Start at the title; pause; skips a cutscene | Enter |
| D-pad | Menus; Up changes the camera in replays | Arrows |

The game has no way to change these. Most of the in-game meanings come from
Shinya Aoki's 2002 [FAQ for the US Xbox version](https://www.gamerevolution.com/?p=32187);
reverse comes from play. Only the throttle, steering, reverse and the menu buttons
have been checked on this port. What Black (E) and Back (Esc) do isn't known.

The keyboard drives the first pad, with or without a controller there, while the
game's window has the focus (`RECOMP_KEYBOARD=0` turns it off).

Any connected controller works, plugged in at any time, and together they count
as one player (the game is single-player).

### Settings

The game's own OPTIONS screen has vibration, music, effects and engine
volume, and a PC SETTINGS row under MUSIC TEST. It opens a page of the same
screen, in the game's own lettering: widescreen, resolution, texture
filtering, anti-aliasing, brighter menus, TV picture and fullscreen (left
and right change them), MORE SETTINGS for everything else, QUIT GAME (A
twice) and EXIT. MORE SETTINGS, Back + Start or F1 open the full settings
menu, anywhere: a menu over the game, which pauses (Display, Graphics,
Audio, Input, Power, Advanced).
A setting tagged RESTART takes effect at the next start. B or Esc closes it.
Graphics > Renderer offers the toolkit's new native renderer, off by default:
it's still being tried out and isn't finished.

What you change there is kept in `settings.toml` beside the saves. The
defaults are `bin/enhance.toml` (copied from `config/enhance.toml` at the
first build; yours is never overwritten), which you can also edit; the
menu's Reset goes back to it. The variable beside each sets it for one run
instead (`RECOMP_RENDER_SCALE=1 bin/wreckless`).

| `enhance.toml` key | Default | What it does | Variable |
|---|---|---|---|
| `render.scale` | `2` | internal resolution: 2 draws everything at twice the size and filters it down | `RECOMP_RENDER_SCALE` |
| `graphics.aniso` | `"16"` | anisotropic filtering: `"off"`, `"2"` to `"16"` | `RECOMP_GFX_ANISO` |
| `graphics.aa` | `"off"` | anti-aliasing of the finished frame: `"fxaa"`, `"smaa"` | `RECOMP_GFX_AA` |
| `present.filter` | `"linear"` | how the window scales: `"nearest"`, `"linear"`, `"integer"` | `RECOMP_PRESENT_FILTER` |
| `present.fullscreen` | `false` | | `RECOMP_PRESENT_FULLSCREEN` |
| `game.widescreen` | `true` | missions, replays and cutscenes in 16:9, the HUD in its own shape; the menus stay 4:3 | `RECOMP_WIDESCREEN` |
| `game.menu_brightness` | `true` | the menus a little brighter than the console's | `RECOMP_MENU_BRIGHTNESS` |
| `game.tv_picture` | `false` | brighter midtones, as a capture of the Xbox on a TV | `RECOMP_TV_PICTURE` |

### What is different from the console

- The 3D is drawn at twice the console's 640x480 and textures are filtered
  anisotropically (both settings above; `render.scale = 1` and
  `graphics.aniso = "off"` are the console's picture).
- Missions, replays and cutscenes are 16:9: the same height of view as the
  console's and more at the sides, as the game draws its own split-screen
  chase panels; the HUD keeps its shape and place. The menus are 4:3, as on
  the console. `game.widescreen = false` is the console's 4:3 throughout.
- It runs at 30 frames a second, the game's own rate, as on the console.

## Status

Early, and not finished. Played by hand in a window with a controller: missions
A-1, A-2 and B-1, and the current build briefly, without problems. The menus,
the attract demo, saving and loading have also been checked by scripted runs.
The game has not been played through to the end.
Linux only, and run so far on an NVIDIA RTX 4070 and a Radeon 890M (RADV). The
Steam Deck build hasn't been played on a Deck.

Known problems:
- The opening cutscene before scenario B ended at once in play sessions; it
  plays in scripted runs.

NOTES.md is the record, ROADMAP.md the plan.

## License

MIT, like sp00nznet/wreckless, whose layout and scripts this started from
(its license is kept as `LICENSE.wreckless`). It covers this repository's own
code only, not the game.
