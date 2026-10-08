#!/bin/bash
# Builds the Amiga executable and a bootable floppy image inside Docker
# (Bebbo's amiga-gcc, see Dockerfile).
# Usage: ./build.sh          -> build out/dnd, out/dnd.pak and out/dnd.adf (bootable disk)
#        ./build.sh clean    -> remove build artifacts
set -euo pipefail
cd "$(dirname "$0")"

IMAGE=dnd-amiga-devkit

if [ "${1:-}" = "clean" ]; then
    rm -rf out
    exit 0
fi

docker build -q -t "$IMAGE" . > /dev/null

TTY=""
[ -t 1 ] && TTY="-t"          # no terminal in CI: docker must not ask for one
docker run --rm $TTY -v "$PWD":/project -w /project "$IMAGE" make "$@"

# Requirement: the game fits on one standard double-density floppy (880 KB), like most A500 games
# did. xdftool can't write past a full disk, so the build fails before; this reports the room left.
if [ "${1:-}" != "clean" ] && [ -f out/dnd.adf ]; then
    docker run --rm -v "$PWD":/project -w /project "$IMAGE" xdftool out/dnd.adf info \
        | awk '/^used:/ { printf "Disk: %d of 880 KB used (%s)\n", $4 / 1024, $5 }'
fi
