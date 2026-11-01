/*
 * recomp_env_game.h - Wreckless's defaults for runtime keys.
 *
 * Read by the runtime's recomp_env.h (RECOMP_ENV_HAVE_GAME_KEYS, set on the
 * recomp_env target in CMakeLists.txt). A default here applies when the
 * environment sets neither the key's new nor its old spelling. The config
 * keys that only the Vulkan build wants stay in main.c's runtime_defaults.
 */
#ifndef WR_RECOMP_ENV_GAME_H
#define WR_RECOMP_ENV_GAME_H

/* The title's own settings, also in enhance.toml's [game] (main.c binds
 * them): TV_PICTURE 1 shows the frames as a television showed the
 * console's (picture.c), 0 as the console put them out. */
#define RECOMP_ENV_GAME_KEYS(X) \
    X(MENU_BRIGHTNESS,   CONFIG, "RECOMP_MENU_BRIGHTNESS",   NULL, "1 (default): the menus a little brighter than the console's (gamma 0.7 while the menu loop runs); 0: as the console (game.menu_brightness)") \
    X(WIDESCREEN,        CONFIG, "RECOMP_WIDESCREEN",        NULL, "1: missions, replays and cutscenes in 16:9 (a wider view; the menus stay 4:3); 0 (default): 4:3 as the console (game.widescreen)") \
    X(TV_PICTURE,        CONFIG, "RECOMP_TV_PICTURE",        NULL, "1: brighter midtones, as a capture of the Xbox game measures; 0 (default): the console's output as it is (game.tv_picture)")

/* APU_DSP_ACK: DirectSound waits on the DSP's command doorbell; the runtime
 *   acknowledges it where it finds it (GPSADDR). sp00nznet/wreckless found
 *   the same wait in this DSOUND (sub_0012A8D1 in DSTEAL_JP.xbe: 3 written
 *   to [ebx+0x810], then a spin until the DSP clears it).
 * USB: the title's own XPP driver runs the pads on the emulated OHCI
 *   controller, so a pad only exists with it. */
#define RECOMP_ENV_GAME_DEFAULTS(D) \
    D(APU_DSP_ACK, "auto") \
    D(USB, "1")

#endif
