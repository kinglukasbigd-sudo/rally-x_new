#!/usr/bin/env bash
# Stage SDL2 development headers into third_party/sdl2 without needing root.
#
# Use this on a machine that already has the SDL2 *runtime* (libSDL2-2.0.so.0)
# but not libsdl2-dev, and where you cannot apt-get install.  If you can
# install packages normally, just do that instead:
#
#     sudo apt-get install libsdl2-dev        # Debian / Ubuntu
#     sudo dnf install SDL2-devel             # Fedora
#     brew install sdl2                       # macOS
#
# The Makefile prefers third_party/sdl2 when it exists and otherwise falls
# back to sdl2-config.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DST="$ROOT/third_party/sdl2"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

RUNTIME="$(ldconfig -p 2>/dev/null | grep -m1 'libSDL2-2.0.so.0' | awk '{print $NF}' || true)"
if [ -z "$RUNTIME" ]; then
    echo "error: SDL2 runtime (libSDL2-2.0.so.0) not found on this system" >&2
    exit 1
fi

echo "runtime: $RUNTIME"
cd "$TMP"
apt-get download libsdl2-dev
ar x ./*.deb
tar xf data.tar.*

mkdir -p "$DST/include" "$DST/lib"
cp -r usr/include/SDL2 "$DST/include/"
# Debian splits the real config header out per architecture.
find usr/include -name '_real_SDL_config.h' -exec cp {} "$DST/include/SDL2/" \;
cp usr/lib/*/libSDL2main.a "$DST/lib/" 2>/dev/null || true
ln -sf "$RUNTIME" "$DST/lib/libSDL2.so"

echo "staged SDL2 headers into $DST"
