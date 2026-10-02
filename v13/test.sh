#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$ROOT/build_v13/tests"
clang++-18 -std=c++11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$ROOT/v13/filesystem.cpp" "$ROOT/v13/library.cpp" "$ROOT/v13/tests/test_library.cpp" \
  -o "$ROOT/build_v13/tests/test_library"
"$ROOT/build_v13/tests/test_library"
clang++-18 -std=c++11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$ROOT/v13/filesystem.cpp" "$ROOT/v13/library.cpp" "$ROOT/v13/goldhen_access.cpp" "$ROOT/v13/tests/test_goldhen.cpp" \
  -o "$ROOT/build_v13/tests/test_goldhen"
"$ROOT/build_v13/tests/test_goldhen"
