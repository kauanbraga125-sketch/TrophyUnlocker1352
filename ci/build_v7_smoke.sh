#!/usr/bin/env bash
set -euxo pipefail

ROOT="$PWD"
DEPS="$ROOT/.deps_v7"
WORK="$ROOT/v7_work"
DIST="$ROOT/dist_v7"
OO_VERSION=v0.5.4
OO_ASSET=toolchain-llvm-18.tar.gz

rm -rf "$DEPS" "$WORK" "$DIST"
mkdir -p "$DEPS" "$WORK" "$DIST"

sudo apt-get update
sudo apt-get install -y clang-18 lld-18 llvm-18 make curl tar unzip file
sudo ln -sf /usr/bin/clang-18 /usr/local/bin/clang
sudo ln -sf /usr/bin/clang++-18 /usr/local/bin/clang++
sudo ln -sf /usr/bin/ld.lld-18 /usr/local/bin/ld.lld
sudo ln -sf /usr/bin/llvm-ar-18 /usr/local/bin/llvm-ar
sudo ln -sf /usr/bin/llvm-ranlib-18 /usr/local/bin/llvm-ranlib
command -v docker

cd "$DEPS"
curl -fL --retry 5 --retry-all-errors \
  "https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/${OO_VERSION}/${OO_ASSET}" \
  -o "$OO_ASSET"
echo "3c7cd5bb593ca74fa1c13fd59f3938dc0fc07985167f7275063019e63abe4526  $OO_ASSET" | sha256sum -c -
tar xzf "$OO_ASSET"
OO_PS4_TOOLCHAIN="$(find "$DEPS" -type f -name link.x -printf '%h\n' | head -n1)"
test -f "$OO_PS4_TOOLCHAIN/link.x"
chmod +x "$OO_PS4_TOOLCHAIN/bin/linux/"* || true
export OO_PS4_TOOLCHAIN

# Start from the unmodified official OpenOrbis dialogs sample.
cp -a "$OO_PS4_TOOLCHAIN/samples/dialogs" "$WORK/dialogs"
cd "$WORK/dialogs"

# Only replace the sample's visible text. Keep the official initialization flow.
python3 - <<'PY'
from pathlib import Path
p = Path('dialogs/main.c')
s = p.read_text()
old = '''if (show_dialog(MDIALOG_YESNO, "Do you like %s?", "OpenOrbis"))
    {
        show_dialog(MDIALOG_OK, "User likes %s :)", "OpenOrbis");
    }
    else
    {
        show_dialog(MDIALOG_OK, "User doesn't like %s :(", "OpenOrbis");
    }'''
new = '''show_dialog(MDIALOG_OK, "Trophy Unlocker 13.52 - V7 smoke test abriu.\\n\\nEste build usa somente o sample oficial OpenOrbis.\\nFeche pelo botao PS.");'''
if old not in s:
    raise SystemExit('official dialog block not found')
s = s.replace(old, new, 1)
p.write_text(s)
PY

# Keep official packaging semantics: CATEGORY=gd, paid=0x380...0011, no custom authinfo.
sed -i 's/^TITLE       :=.*/TITLE       := Trophy Unlocker 13.52 V7 Smoke/' Makefile
sed -i 's/^TITLE_ID    :=.*/TITLE_ID    := BREW13525/' Makefile
sed -i 's/^CONTENT_ID  :=.*/CONTENT_ID  := IV0000-BREW13525_00-TROPHYSMOKEV7000/' Makefile
grep -q "CATEGORY --type Utf8 --maxsize 4 --value 'gd'" Makefile
! grep -q -- '--authinfo' Makefile
grep -q 'for(;;);' dialogs/main.c
grep -q 'V7 smoke test abriu' dialogs/main.c

# Run stock OpenOrbis PkgTool with the runtime it was built for.
TOOL="$OO_PS4_TOOLCHAIN/bin/linux/PkgTool.Core"
mv "$TOOL" "$TOOL.real"
docker pull mcr.microsoft.com/dotnet/core/runtime:3.1-bionic
cat > "$TOOL" <<EOF
#!/usr/bin/env bash
set -e
exec docker run --rm --user "$(id -u):$(id -g)" \
  -e DOTNET_ROLL_FORWARD=Minor -e DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 \
  -v /home/runner/work:/home/runner/work -w "\$PWD" \
  mcr.microsoft.com/dotnet/core/runtime:3.1-bionic \
  "$OO_PS4_TOOLCHAIN/bin/linux/PkgTool.Core.real" "\$@"
EOF
chmod +x "$TOOL"
"$TOOL" version

make clean || true
make
PKG="IV0000-BREW13525_00-TROPHYSMOKEV7000.pkg"
test -s "$PKG"
"$TOOL" sfo_listentries sce_sys/param.sfo
"$TOOL" pkg_validate --verbose "$PKG"

cp "$PKG" "$DIST/Trophy_Unlocker_13.52_V7_SMOKE.pkg"
sha256sum "$DIST/Trophy_Unlocker_13.52_V7_SMOKE.pkg" > "$DIST/SHA256SUMS.txt"
file "$DIST/Trophy_Unlocker_13.52_V7_SMOKE.pkg"
ls -lh "$DIST"
