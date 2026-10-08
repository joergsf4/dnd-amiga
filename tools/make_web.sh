#!/bin/bash
# Assembles site/ for the browser version (GitHub Pages): web/index.html plus the built disk
# image (out/dnd.adf) as dnd.adf. The emulator itself (vAmigaWeb, WASM) is loaded from
# https://vAmigaWeb.github.io/ in an iframe, not vendored here -- see web/LICENSES.txt.
# Build the disk first (./build.sh). Test locally: (cd site && python3 -m http.server 8000), but
# vAmigaWeb.github.io can only fetch dnd.adf cross-origin from a real https origin (GitHub Pages
# sends Access-Control-Allow-Origin: *; localhost doesn't), so a true test needs the deployed site.
set -euo pipefail
cd "$(dirname "$0")/.."

[ -f out/dnd.adf ] || { echo "build first: ./build.sh" >&2; exit 1; }
mkdir -p site
cp web/index.html web/LICENSES.txt site/
cp out/dnd.adf site/dnd.adf
