# Headless Amiga emulator for automated screenshots (tools/emutest.sh):
# FS-UAE on a virtual X display, screenshots with ImageMagick, input with xdotool.
FROM debian:trixie-slim

RUN sed -i "s/Components: main/Components: main contrib/" /etc/apt/sources.list.d/debian.sources \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        fs-uae xvfb xdotool imagemagick libgl1-mesa-dri \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /project
