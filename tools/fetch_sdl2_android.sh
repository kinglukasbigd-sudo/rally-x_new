#!/usr/bin/env bash
# Fetch the SDL2 source tree that the Android build compiles from.
#
# The desktop build links against the system SDL2; the Android build has to
# compile SDL itself, so it needs the full source. This is SDL2 under the zlib
# licence, unmodified -- see third_party/SDL2-*/LICENSE.txt.
set -euo pipefail

VERSION="${1:-2.30.9}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/third_party/SDL2-$VERSION"

if [ -d "$DEST" ]; then
    echo "already present: $DEST"
    exit 0
fi

URL="https://github.com/libsdl-org/SDL/releases/download/release-$VERSION/SDL2-$VERSION.tar.gz"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "fetching $URL"
curl -fsSL -o "$TMP/SDL2.tar.gz" "$URL"
mkdir -p "$ROOT/third_party"
tar xzf "$TMP/SDL2.tar.gz" -C "$ROOT/third_party"

echo "SDL2 $VERSION unpacked into $DEST"
echo "note: android/app/src/main/cpp/CMakeLists.txt pins this version -- update"
echo "      SDL_ROOT there if you fetch a different one."
