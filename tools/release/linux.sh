#!/usr/bin/env bash
# tools/release/linux.sh OUT_DIR: the Linux download (an AppImage), with no part of the game in it.
#
# It needs, in third_party/ (or a link to it): zig-x86_64-linux-0.17.0, sdl3-rel (static SDL3),
# python-standalone/python (Python 3.14 in one program), pack-venv (with Capstone),
# appimagetool-x86_64.AppImage and appimage-runtime-x86_64; podman with this repo's build
# image (deck-build.sh); the toolkit (XBOXRECOMP_DIR, default as regen.sh); and the game
# files in game/, once, for the translation's hash the runtime is checked against.
#
# The runtime and this game's own code are built in the Ubuntu 24.04 container, so the game needs
# glibc 2.38. The player's Zig compiles the translation with the same flags and links them.
# The system's libstdc++, Vulkan, X11, Wayland, audio and so on are used, not bundled: a program's
# copy of those can clash with the graphics driver's. The launcher is built with Zig for glibc 2.31.
set -euo pipefail
export LC_ALL=C
cd "$(dirname "$0")/../.."
ROOT=$PWD
OUT=$(realpath -m "${1:?usage: linux.sh OUT_DIR}")
TP=$ROOT/third_party
RT=$(cd "${XBOXRECOMP_DIR:-$ROOT/../antbox}" && pwd)
ZDIR=$TP/zig-x86_64-linux-0.17.0; Z=$ZDIR/zig; SDL=$TP/sdl3-rel; PYS=$TP/python-standalone/python
XBE="$ROOT/game/Wreckless - The Yakuza Missions/default.xbe"
LIBDIR=/usr/lib/x86_64-linux-gnu
IMAGE="${WR_IMAGE:-localhost/ds2-build:24.04}"; podman image exists "$IMAGE" || IMAGE=localhost/wr-build:24.04
REL=$(cat tools/release/VERSION)
B=$OUT/AppDir/usr/lib/wreckless-recompiled
rm -rf "$OUT/AppDir"; mkdir -p "$B/lib/so" "$B/lib/link" "$B/include/game/recomp" "$B/config" "$OUT/AppDir/usr/bin"

echo "== the runtime and this game's own code, in the container"
# (mounted at /src, so what the build records names no folder of this machine)
rm -rf build/release build/release.*
podman run --rm -v "$ROOT:/src/wreckless" -v "$RT:/src/antbox" -w /src/wreckless -e XBOXRECOMP_DIR=/src/antbox \
    -e WR_BUILD_DIR=/src/wreckless/build/release "$IMAGE" bash /src/wreckless/container/build.sh
D=build/release
for f in main options_row pc_page picture settings widescreen recomp/recomp_hooks recomp/recomp_manual recomp/recomp_native; do
    cp "$D/src/game/CMakeFiles/wreckless.dir/$f.c.o" "$B/lib/$(basename $f).o"
done
for a in audio/libxbox_dsound nv2a/libxbox_nv2a kernel/libxbox_kernel video/libxbox_video enhance/libxbox_enhance \
         overlay/libxbox_overlay usb/libxbox_usb d3d/libxbox_d3d8 input/libxbox_input apu/libxbox_apu \
         install/libxbox_install soundtrack/libxbox_soundtrack platform/libplatform platform/librecomp_env; do
    cp "$D/xboxrecomp/src/$a.a" "$B/lib/"
done
strip --strip-debug "$B"/lib/*.o "$B"/lib/*.a
echo "== what the download carries from the container"
podman run --rm -v "$B:/out" "$IMAGE" bash -c '
  L=/usr/lib/x86_64-linux-gnu
  cp $L/libglslang.a $L/libglslang-default-resource-limits.a $L/libSPIRV-Tools-opt.a $L/libSPIRV-Tools.a /out/lib/
  for s in libSDL2-2.0.so.0 libepoxy.so.0 libcrypto.so.3 libsamplerate.so.0 libdecor-0.so.0; do cp -L $L/$s /out/lib/so/; done
  cp -L $L/libstdc++.so.6 $L/libgcc_s.so.1 /out/lib/link/
  mkdir -p /out/licenses
  for x in SDL2:libsdl2-2.0-0 epoxy:libepoxy0 OpenSSL:libssl3t64 samplerate:libsamplerate0 decor:libdecor-0-0; do
      cp /usr/share/doc/${x#*:}/copyright /out/licenses/${x%%:*}-copyright.txt
  done
  dpkg-query -W libsdl2-2.0-0 libepoxy0 libssl3t64 libsamplerate0 libdecor-0-0 > /out/licenses/library-versions.txt'
{ for o in main options_row pc_page picture settings widescreen recomp_hooks recomp_manual recomp_native; do echo "$o.o"; done
  echo -Wl,--start-group
  for a in libxbox_dsound libxbox_nv2a libxbox_kernel libxbox_video libxbox_enhance libxbox_overlay libxbox_usb libxbox_d3d8 \
           libxbox_input libxbox_apu libxbox_install libxbox_soundtrack libplatform librecomp_env \
           libglslang libglslang-default-resource-limits libSPIRV-Tools-opt libSPIRV-Tools; do echo "$a.a"; done
  echo -Wl,--end-group
  for s in libSDL2-2.0.so.0 libepoxy.so.0 libcrypto.so.3; do echo "so/$s"; done
  echo link/libstdc++.so.6; echo link/libgcc_s.so.1
  echo -lm; echo -ldl; echo -pthread
  echo -Wl,--allow-shlib-undefined; echo -Wl,--disable-new-dtags; echo '-Wl,-rpath,$ORIGIN/lib'
} > "$B/lib/link.txt"

echo "== the headers the translated code includes"
(cd "$RT" && find include src -name '*.h' -print0 | tar --null -T - -cf -) | { mkdir -p "$B/include/antbox"; tar -C "$B/include/antbox" -xf -; }
cp src/game/*.h "$B/include/game/"
cp config/enhance.toml "$B/enhance.toml"
cp tools/release/game.manifest.sha256 "$B/"
echo "wreckless-recompiled $REL $(date -u +%Y-%m-%d) $(git rev-parse --short HEAD)" > "$B/VERSION"

echo "== the translator: the toolkit's tools on a standalone Python with Capstone"
mkdir -p "$B/python/bin" "$B/python/lib" "$B/antbox"
cp -a "$PYS/bin/python3.14" "$B/python/bin/"
tar -C "$PYS/lib" --exclude=test --exclude=idlelib --exclude=tkinter --exclude=turtledemo --exclude=ensurepip \
    --exclude=pydoc_data --exclude='config-3.14-*' --exclude='site-packages/*' --exclude='_tkinter.*' \
    --exclude='_dbm.*' -cf - python3.14 | tar -C "$B/python/lib" -xf -
mkdir -p "$B/python/lib/python3.14/site-packages"
cp -r "$TP/pack-venv/lib/python3.14/site-packages/capstone" "$B/python/lib/python3.14/site-packages/"
tar -C "$RT" --exclude=__pycache__ --exclude=nvr_replay --exclude='test_*' -cf - tools templates | tar -C "$B/antbox" -xf -
find "$B/python" -name __pycache__ -prune -exec rm -r {} +
cp config/seed_functions.json config/seeds_crosscheck.json config/manual_functions.json config/spin_waits.json src/game/recomp/recomp_hooks.c "$B/config/"
cp tools/release/translate.py "$B/"
cat > "$B/translate" <<'EOF'
#!/bin/sh
# default.xbe to C: translate.py on the Python beside it (-E -s: none of the player's Python settings)
H=$(dirname "$(readlink -f "$0")")
exec "$H/python/bin/python3.14" -E -s -B "$H/translate.py" "$@"
EOF
chmod +x "$B/translate"

echo "== the translation's hash, from this repo's own copy of the game (the hash goes in, the game doesn't)"
T=$(mktemp -d "$ROOT/work/rel-hash.XXXX")
"$B/translate" "$XBE" "$T" --hash | tail -1 > "$B/GEN_SHA256"
if ! diff -rq "$T/gen" src/game/recomp/gen > /dev/null; then echo "the bundled translator differs from regen.sh's result"; exit 1; fi
rm -rf "$T"
cat "$B/GEN_SHA256"

echo "== the launcher"
TOOLS=$ROOT/build/rel-tools; mkdir -p "$TOOLS"
for spec in freetype:libfreetype.so.6; do
    name=${spec%%:*} so=${spec#*:}
    nm -D --defined-only "$LIBDIR/$so" | awk '$2 ~ /^[TWi]$/ {print $3}' | sed 's/@.*//' | sort -u | sed 's/.*/void &(void) {}/' > "$TOOLS/stub_$name.c"
    "$Z" cc -target x86_64-linux-gnu.2.31 -shared -fPIC -o "$TOOLS/lib$name.so" -Wl,-soname,"$so" "$TOOLS/stub_$name.c"
done
"$Z" cc -target x86_64-linux-gnu.2.31 -O2 -fPIE -pie -I"$SDL/include" -I"$RT/src" -I/usr/include/freetype2 \
    -idirafter /usr/include -idirafter /usr/include/x86_64-linux-gnu -o "$OUT/AppDir/usr/bin/wreckless-recompiled" \
    launcher/launcher.c launcher/sha256.c "$SDL/lib/libSDL3.a" -L"$TOOLS" -lfreetype -lm -ldl -pthread -Wl,--allow-shlib-undefined

strip "$OUT/AppDir/usr/bin/wreckless-recompiled"
echo "== Zig"
cp -a "$(readlink -f "$ZDIR")" "$B/zig"; rm -rf "$B/zig/doc"

echo "== the licences (every part the download carries)"
L=$B/licenses; mkdir -p "$L"
cp LICENSE "$L/LICENSE"; cp LICENSE.wreckless "$L/"
cp "$RT/LICENSE" "$L/antbox-LICENSE"; [ -f "$RT/NOTICE" ] && cp "$RT/NOTICE" "$L/antbox-NOTICE"
cp -r "$RT/LICENSES" "$L/antbox-LICENSES"
cp "$RT/third_party/roboto_condensed/OFL.txt" "$L/RobotoCondensed-OFL.txt"
cp "$TP"/SDL3-*/LICENSE.txt "$L/SDL3-LICENSE.txt"
cp "$ZDIR/LICENSE" "$L/Zig-LICENSE.txt"
for x in libcxx libcxxabi libunwind; do cp "$ZDIR/lib/$x/LICENSE.TXT" "$L/LLVM-$x-LICENSE.txt"; done
cp "$TP"/pack-venv/lib/python3.14/site-packages/capstone-*.dist-info/LICENSE.TXT "$L/Capstone-LICENSE.txt"
cp "$PYS/lib/python3.14/LICENSE.txt" "$L/Python-LICENSE.txt"
cp tools/release/python-builtins-NOTICE.txt "$L/"

echo "== nothing in it names this machine"
if grep -rlaE "$HOME|$(whoami)" "$OUT/AppDir"; then echo "the files above name this machine"; exit 1; fi
echo "== AppImage"
A=$OUT/AppDir
cat > "$A/AppRun" <<'EOF'
#!/bin/sh
HERE=$(dirname "$(readlink -f "$0")")
exec "$HERE/usr/bin/wreckless-recompiled" "$@"
EOF
chmod +x "$A/AppRun"
cat > "$A/wreckless-recompiled.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Wreckless: Recompiled
Comment=The Xbox game from 2002, rebuilt from your own copy
Exec=wreckless-recompiled
Icon=wreckless-recompiled
Categories=Game;
EOF
cp tools/release/icon.png "$A/wreckless-recompiled.png"
ARCH=x86_64 "$TP/appimagetool-x86_64.AppImage" --no-appstream --runtime-file "$TP/appimage-runtime-x86_64" "$A" "$OUT/Wreckless_Recompiled-$REL-x86_64.AppImage"
ls -la "$OUT/Wreckless_Recompiled-$REL-x86_64.AppImage"
