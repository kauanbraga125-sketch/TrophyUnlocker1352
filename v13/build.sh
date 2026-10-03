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

# Professional branding: menu icon + clean background.
mkdir -p "$WORK/sce_sys"
rsvg-convert -w 512 -h 512 "$ROOT/v13/branding/icon0.svg" -o "$WORK/sce_sys/icon0-rgba.png"
rsvg-convert -w 1920 -h 1080 "$ROOT/v13/branding/pic0.svg" -o "$WORK/sce_sys/pic0-rgba.png"
convert "$WORK/sce_sys/icon0-rgba.png" -alpha off -type TrueColor PNG24:"$WORK/sce_sys/icon0.png"
convert "$WORK/sce_sys/pic0-rgba.png" -alpha off -type TrueColor PNG24:"$WORK/sce_sys/pic0.png"
rm -f "$WORK/sce_sys/icon0-rgba.png" "$WORK/sce_sys/pic0-rgba.png"
python3 - "$WORK/sce_sys/icon0.png" 512 512 "$WORK/sce_sys/pic0.png" 1920 1080 <<'PY'
import struct, sys
for path, ew, eh in ((sys.argv[1], int(sys.argv[2]), int(sys.argv[3])), (sys.argv[4], int(sys.argv[5]), int(sys.argv[6]))):
    with open(path, 'rb') as f: sig=f.read(24)
    if sig[:8] != b'\x89PNG\r\n\x1a\n': raise SystemExit(f'Not a PNG: {path}')
    w,h=struct.unpack('>II',sig[16:24])
    if (w,h)!=(ew,eh): raise SystemExit(f'Bad dimensions for {path}: {w}x{h}, expected {ew}x{eh}')
    print(f'[OK] branding {path}: {w}x{h}')
PY

# Audio assets are generated deterministically during the build.
python3 "$ROOT/v13/generate_audio.py" "$WORK/assets/audio"

rm -f "$WORK/SDL2/"*.cpp "$WORK/SDL2/"*.h
cp "$ROOT/v13/"*.cpp "$ROOT/v13/"*.h "$WORK/SDL2/"
python3 "$ROOT/v13/patch_trophy_ui.py" "$WORK/SDL2/main.cpp"
python3 "$ROOT/v13/patch_audio.py" "$WORK/SDL2/main.cpp"
cp "$ROOT/v13/Makefile" "$WORK/Makefile"
make -C "$WORK" clean
make -C "$WORK" -j2
TOOL="$OO_PS4_TOOLCHAIN/bin/linux/PkgTool.Core"
PKG="$WORK/IV0000-BREW13533_00-TROPHYV12UI01200.pkg"
"$TOOL" pkg_validate --verbose "$PKG" | tee "$DIST/pkg-validation.txt"
if rg -i '\[(FAIL|ERROR)\]' "$DIST/pkg-validation.txt"; then exit 1; fi
test "$(rg -c '^\[OK\]' "$DIST/pkg-validation.txt")" -ge 28
"$TOOL" sfo_listentries "$WORK/sce_sys/param.sfo" > "$DIST/param-sfo.txt"
OUT="Trophy_Unlocker_13.52_V13_10_Professional_Audio.pkg"
cp "$PKG" "$DIST/$OUT"
(cd "$DIST" && sha256sum "$OUT" > SHA256SUMS.txt)
