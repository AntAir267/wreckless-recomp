#!/usr/bin/env bash
# deck-build.sh - a build that also runs on SteamOS (glibc 2.41): the game
# compiled inside an Ubuntu 24.04 container (container/Containerfile) into
# build/deck. Needs podman. It uses Double-S.T.E.A.L 2's image when it is
# there (the same Containerfile); otherwise it makes localhost/wr-build:24.04
# from container/Containerfile, which downloads Ubuntu's packages once.
#   ./deck-build.sh && WR_BIN=build/deck/bin/wreckless ./package.sh DEST
set -euo pipefail
WR="$(cd "$(dirname "$0")" && pwd)"
RT="$(cd "${XBOXRECOMP_DIR:-$WR/../antbox}" && pwd)"
IMAGE="${WR_IMAGE:-localhost/ds2-build:24.04}"
if ! podman image exists "$IMAGE"; then
    IMAGE=localhost/wr-build:24.04
    podman image exists "$IMAGE" || podman build -t "$IMAGE" -f "$WR/container/Containerfile" "$WR/container"
fi
mkdir -p "$WR/build"
podman run --rm -v "$WR:$WR" -v "$RT:$RT" -w "$WR" -e XBOXRECOMP_DIR="$RT" \
    "$IMAGE" bash "$WR/container/build.sh"
