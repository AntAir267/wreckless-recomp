cmake_minimum_required(VERSION 3.20)
project(wreckless_recomp C)

# Wreckless: The Yakuza Missions (Xbox, 2002) - Static Recompilation
#
# Built the way the Double-S.T.E.A.L 2 recomp and
# sp00nznet/wreckless build: the xboxrecomp toolkit is a dependency beside
# this checkout (../antbox: antbox, the shared Xbox
# toolkit, at the commit in ANTBOX_PIN), not a copy. This repository holds
# only the per-title work.

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)

set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../antbox"
    CACHE PATH "Path to the xboxrecomp toolkit")

# Off Windows the kernel links OpenSSL::Crypto but finds it in its own
# subdirectory, where the imported target is invisible to the executable;
# finding it here first puts it in scope for both.
if(NOT WIN32)
    find_package(OpenSSL REQUIRED)
endif()

# The runtime's opt-in enhancements layer (enhance.toml: render scale, the
# dashboard's HDTV and widescreen settings, the window's filter and aspect).
# On by default for this title; the toolkit's own default is off.
if(NOT DEFINED CACHE{XBOXRECOMP_ENHANCE})
    set(XBOXRECOMP_ENHANCE ON CACHE BOOL "Build the opt-in enhancements layer (src/enhance)")
endif()
# The runtime's in-game settings overlay (Back + Start, F1; src/game/
# settings.c). It needs C++ and SDL2.
if(NOT DEFINED CACHE{XBOXRECOMP_OVERLAY})
    set(XBOXRECOMP_OVERLAY ON CACHE BOOL "Build the in-game settings overlay (Dear ImGui)")
endif()

add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)

# This title's defaults for runtime keys (src/game/env/recomp_env_game.h).
# PUBLIC, so every part of the runtime reads the same table.
if(TARGET recomp_env AND EXISTS "${XBOXRECOMP_DIR}/src/platform/recomp_env.h")
    target_compile_definitions(recomp_env PUBLIC RECOMP_ENV_HAVE_GAME_KEYS)
    target_include_directories(recomp_env PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/game/env)
endif()
add_subdirectory(src/game)
