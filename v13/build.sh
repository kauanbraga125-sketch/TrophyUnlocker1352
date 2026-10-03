#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${OO_PS4_TOOLCHAIN:?Set OO_PS4_TOOLCHAIN or run ci/build_v13.sh}"
WORK="$ROOT/build_v13/app"
DIST="$ROOT/dist_v13"
mkdir -p "$WORK/SDL2" "$DIST"
bash "$ROOT/v13/test.sh" | tee "$DIST/host-tests.txt"
cp -a "$OO_PS4_TOOLCHAIN/samples/SDL2/assets" "$WORK/"
cp -a "$OO_PS4_TOOLCHAIN/samples/SDL2/sce_module" "$WORK/"
cp -a "$OO_PS4_TOOLCHAIN/samples/SDL2/sce_sys" "$WORK/"
rm -f "$WORK/SDL2/"*.cpp "$WORK/SDL2/"*.h
cp "$ROOT/v13/"*.cpp "$ROOT/v13/"*.h "$WORK/SDL2/"
python3 "$ROOT/v13/patch_trophy_ui.py" "$WORK/SDL2/main.cpp"
cp "$ROOT/v13/Makefile" "$WORK/Makefile"
make -C "$WORK" clean
make -C "$WORK" -j2
TOOL="$OO_PS4_TOOLCHAIN/bin/linux/PkgTool.Core"
PKG="$WORK/IV0000-BREW13533_00-TROPHYV12UI01200.pkg"
"$TOOL" pkg_validate --verbose "$PKG" | tee "$DIST/pkg-validation.txt"
if rg -i '\[(FAIL|ERROR)\]' "$DIST/pkg-validation.txt"; then exit 1; fi
test "$(rg -c '^\[OK\]' "$DIST/pkg-validation.txt")" -eq 28
"$TOOL" sfo_listentries "$WORK/sce_sys/param.sfo" > "$DIST/param-sfo.txt"
OUT="Trophy_Unlocker_13.52_V13_10_Capas_Vinculo_Manual.pkg"
cp "$PKG" "$DIST/$OUT"
(cd "$DIST" && sha256sum "$OUT" > SHA256SUMS.txt)
