# Toolchain image for the D&D Dungeon Crawler Amiga port.
# Base: Bebbo's amiga-gcc (m68k-amigaos-gcc 6.5, vasm, NDK 1.3 + 3.x headers,
# libnix, xdftool, lha), multi-arch, so it runs natively on Apple Silicon.
# On top: Pillow for the asset tools (the dnd tools/*.py generate the graphics),
# and the ZX0 compressor (Einar Saukas, BSD-3) for the data file (tools/make_res.py;
# src/amiga/unzx0.c unpacks it). In this image gcc and ld are the Amiga ones, so the
# compressor is built with the native compiler and linker by their full names.
FROM sacredbanana/amiga-compiler:m68k-amigaos

RUN apt-get update \
    && apt-get install -y --no-install-recommends python3-pil \
    && rm -rf /var/lib/apt/lists/*

RUN git clone -q https://github.com/einar-saukas/ZX0.git /opt/zx0 \
    && git -C /opt/zx0 checkout -q ecde3a2 \
    && A=$(uname -m)-linux-gnu && mkdir /tmp/native \
    && ln -s /usr/bin/$A-ld /tmp/native/ld && ln -s /usr/bin/$A-as /tmp/native/as \
    && $A-gcc -B/tmp/native/ -O2 -o /usr/local/bin/zx0 \
        /opt/zx0/src/zx0.c /opt/zx0/src/optimize.c /opt/zx0/src/compress.c /opt/zx0/src/memory.c \
    && rm -rf /tmp/native

WORKDIR /project
