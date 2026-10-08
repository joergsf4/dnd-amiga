#!/bin/bash
# Plays the game headlessly (FS-UAE on a virtual X display, in Docker) and saves screenshots to
# out/emutest/, so results can be checked without looking at a live window. The game's debug
# output (kprintf, serial port) lands in out/emutest/serial.txt, the sound in out/emutest/audio.wav.
#
# Usage: tools/emutest.sh [step ...]
#   @T          wait until T seconds after power-on
#   +T          wait T more seconds (fractions allowed)
#   key:K       press key K for 150 ms (X key names: Return, Up, Down, Left, Right, Escape,
#               space, a, b, c, m ...); the game reads the Amiga keyboard
#   hold:K:T    hold key K for T seconds
#   shot:NAME   screenshot to out/emutest/NAME.png
#   wait:TEXT[:N]  wait until the game's serial output contains TEXT (N times; at most 3 minutes)
#   N           (a plain number) shorthand for "@N shot:tNN"
#   Default: 30.
#
# Boots from out/hd (the files of the disk as a hard drive folder: much faster than the floppy,
# which loads at real speed). ADF=1 boots out/dnd.adf instead.
# Example: tools/emutest.sh @20 shot:title key:Return +2 shot:create
set -euo pipefail
cd "$(dirname "$0")/.."

IMAGE=dnd-amiga-emutest
docker build -q -t "$IMAGE" -f docker/emutest.Dockerfile . > /dev/null

# Without a Kickstart ROM FS-UAE uses AROS, which takes 400-500 KB of the A500's memory itself
# (Kickstart 1.3: ~40 KB); the machine gets 1 MB of fast RAM on top then, so that the game fits.
KICK="internal"
EXTRA="--fast_memory=1024"
ROM=$(ls roms/*.rom 2>/dev/null | head -1 || true)
if [ -n "$ROM" ]; then KICK="/project/$ROM"; EXTRA=""; fi
MEDIA="--hard_drive_0=/project/out/hd"
if [ "${ADF:-}" = "1" ]; then MEDIA="--floppy_drive_0=/project/out/dnd.adf"; fi

mkdir -p out/emutest
rm -f out/emutest/*.png
docker run --rm -i -v "$PWD":/project -w /project \
    -e KICK="$KICK" -e EXTRA="$EXTRA" -e MEDIA="$MEDIA" -e HOME=/tmp \
    "$IMAGE" bash -s -- "${@:-30}" <<'EOF'
set -e
Xvfb :99 -screen 0 1024x800x24 > /dev/null 2>&1 &
export DISPLAY=:99
sleep 1
: > out/emutest/serial.txt
# The sound goes into out/emutest/audio.wav (OpenAL's file backend) for checking it by analysis.
printf '[general]\ndrivers = wave\n[wave]\nfile = /project/out/emutest/audio.wav\n' > /tmp/alsoft.conf
export ALSOFT_CONF=/tmp/alsoft.conf
start=$(date +%s.%N)
fs-uae amiga/a500.fs-uae $MEDIA $EXTRA --kickstart_file="$KICK" \
    --fullscreen=0 --window_width=768 --window_height=576 \
    --automatic_input_grab=0 --floppy_drive_volume=0 \
    --serial_port=/project/out/emutest/serial.txt > out/emutest/fs-uae.log 2>&1 &
win=""
window() {
    [ -n "$win" ] || win=$(xdotool search --sync --name "FS-UAE" | head -1)
    echo "$win"
}
waitUntil() { sleep "$(awk -v t="$1" -v s="$start" -v n="$(date +%s.%N)" 'BEGIN { d = t - (n - s); print (d > 0) ? d : 0 }')"; }
for step in "$@"; do
    case "$step" in
        @*)      waitUntil "${step#@}" ;;
        +*)      sleep "${step#+}" ;;
        key:*)   xdotool windowfocus --sync "$(window)" keydown "${step#key:}"
                 sleep 0.15; xdotool keyup "${step#key:}"; sleep 0.1 ;;
        hold:*)  k=${step#hold:}; xdotool windowfocus --sync "$(window)" keydown "${k%%:*}"
                 sleep "${k#*:}"; xdotool keyup "${k%%:*}" ;;
        wait:*)  w=${step#wait:}; n=1
                 case "$w" in *:[0-9]) n=${w##*:}; w=${w%:*} ;; esac
                 for _ in $(seq 1 1800); do
                     [ "$(LC_ALL=C grep -a -c -F "$w" out/emutest/serial.txt)" -ge "$n" ] && break
                     sleep 0.1
                 done ;;
        shot:*)  import -window "$(window)" "out/emutest/${step#shot:}.png"
                 echo "out/emutest/${step#shot:}.png" ;;
        *)       waitUntil "$step"; n=$(printf "t%02d" "$step")
                 import -window "$(window)" "out/emutest/$n.png"; echo "out/emutest/$n.png" ;;
    esac
done
cp /tmp/FS-UAE/Cache/Logs/fs-uae.log.txt out/emutest/fs-uae-full.log 2>/dev/null || true
EOF
