#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${OO_PS4_TOOLCHAIN:?Set the SDK path for the same V12 font}"
mkdir -p "$ROOT/build_v13/previews"
clang++-18 -std=c++11 -Wall -Wextra -Werror -DTU_HOST_PREVIEW \
  "$ROOT/v13/main.cpp" "$ROOT/v13/library.cpp" "$ROOT/v13/filesystem.cpp" "$ROOT/v13/tests/preview_fs.cpp" \
  $(pkg-config --cflags --libs sdl2 SDL2_image freetype2) -o "$ROOT/build_v13/ui-preview"
export SDL_VIDEODRIVER=dummy
export TU_PREVIEW_FONT="$OO_PS4_TOOLCHAIN/samples/SDL2/assets/fonts/VeraMono.ttf"
for state in library browser cusa details diagnostics empty; do
  export TU_PREVIEW_SCREEN="$state"
  export TU_PREVIEW_OUTPUT="$ROOT/build_v13/previews/$state.bmp"
  if [[ "$state" == empty ]]; then export TU_PREVIEW_EMPTY=1; else unset TU_PREVIEW_EMPTY; fi
  "$ROOT/build_v13/ui-preview"
done
