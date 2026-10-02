#!/usr/bin/env bash
set -euxo pipefail

ROOT="$PWD"
DEPS="$ROOT/.deps"
OO_VERSION=v0.5.4
OO_ASSET=toolchain-llvm-18.tar.gz
mkdir -p "$DEPS"

# Reconstruct the original project and apply the runtime workaround.
cat source_parts/source.part.* | base64 -d > /tmp/tu1352_source.zip
unzip -t /tmp/tu1352_source.zip
unzip -o /tmp/tu1352_source.zip -d "$ROOT"
python3 patches/patch_v4.py
! sed -n '/int main(void)/,$p' installer/src/main.c | grep -q 'return '
grep -q 'idle_forever' installer/src/main.c
grep -q 'TrophyUnlocker1352.log' installer/src/main.c
chmod +x scripts/*.sh || true

sudo apt-get update
sudo apt-get install -y clang-18 lld-18 llvm-18 make git curl tar xz-utils unzip python3 file
sudo ln -sf /usr/bin/clang-18 /usr/local/bin/clang
sudo ln -sf /usr/bin/clang++-18 /usr/local/bin/clang++
sudo ln -sf /usr/bin/ld.lld-18 /usr/local/bin/ld.lld
sudo ln -sf /usr/bin/llvm-ar-18 /usr/local/bin/llvm-ar
sudo ln -sf /usr/bin/llvm-ranlib-18 /usr/local/bin/llvm-ranlib
make -C host_test clean run

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
cd "$ROOT"

# Use the same homebrew packaging pieces as the OpenOrbis hello-world sample.
mkdir -p installer/sce_sys/about installer/sce_module
cp "$OO_PS4_TOOLCHAIN/samples/hello_world/sce_sys/about/right.sprx" installer/sce_sys/about/right.sprx
cp "$OO_PS4_TOOLCHAIN/samples/hello_world/sce_module/libc.prx" installer/sce_module/libc.prx
cp "$OO_PS4_TOOLCHAIN/samples/hello_world/sce_module/libSceFios2.prx" installer/sce_module/libSceFios2.prx

# Pull the public homebrew authinfo value used by Payload Guest instead of embedding it here.
curl -fsSL https://raw.githubusercontent.com/Al-Azif/ps4-payload-guest/main/Makefile -o /tmp/payload_guest.mk
AUTHINFO="$(awk '/^AUTHINFO[[:space:]]*:=/ {print $3; exit}' /tmp/payload_guest.mk)"
test -n "$AUTHINFO"

cat > installer/Makefile <<'MAKEFILE'
TITLE       := Trophy Unlocker 13.52 Manager V4
VERSION     := 1.00
TITLE_ID    := BREW13522
CONTENT_ID  := IV0000-BREW13522_00-TROPHYUNLOCK1352
AUTHINFO    := __AUTHINFO__
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
ODIR        := build
SDIR        := src
LIBS        := -lc -lkernel -lScePad -lSceUserService
LIBMODULES  := $(wildcard sce_module/*)
CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c -O2 -std=gnu11 -Wall -Wextra -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -I$(SDIR)
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o
CFILES      := $(wildcard $(SDIR)/*.c)
OBJS        := $(patsubst $(SDIR)/%.c,$(ODIR)/%.o,$(CFILES))
UNAME_S     := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
CC          := clang
LD          := ld.lld
CDIR        := linux
endif
all: $(CONTENT_ID).pkg
$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .
pkg.gp4: eboot.bin trophy_unlocker_1352.prx sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png $(LIBMODULES)
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"
sce_sys/param.sfo: Makefile
	@mkdir -p sce_sys
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gde'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'
eboot.bin: $(ODIR) $(OBJS)
	$(LD) $(ODIR)/*.o -o $(ODIR)/eboot.elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(ODIR)/eboot.elf -out=$(ODIR)/eboot.oelf --eboot "eboot.bin" --paid 0x3800000000000011 --authinfo=$(AUTHINFO)
$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<
$(ODIR):
	@mkdir -p $@
clean:
	rm -rf build eboot.bin pkg.gp4 sce_sys/param.sfo *.pkg
.PHONY: all clean
MAKEFILE
sed -i "s/__AUTHINFO__/$AUTHINFO/" installer/Makefile

# Run the stock OpenOrbis PkgTool under its expected .NET Core runtime.
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

# Build the GoldHEN plugin.
git clone --depth 1 https://github.com/GoldHEN/GoldHEN_Plugins_SDK.git "$DEPS/GoldHEN_Plugins_SDK"
git clone --depth 1 https://github.com/GoldHEN/GoldHEN_Plugins_Repository.git "$DEPS/GoldHEN_Plugins_Repository"
export GOLDHEN_SDK="$DEPS/GoldHEN_Plugins_SDK"
make -C "$GOLDHEN_SDK" PRINTF=1
./scripts/install_into_goldhen_repo.sh "$DEPS/GoldHEN_Plugins_Repository"
GH_REPO="$DEPS/GoldHEN_Plugins_Repository"
{
  echo '#pragma once'
  echo "#define GIT_COMMIT \"$(git -C "$GH_REPO" rev-parse HEAD)\""
  echo "#define GIT_VER \"$(git -C "$GH_REPO" branch --show-current)\""
  echo "#define GIT_NUM $(git -C "$GH_REPO" rev-list HEAD --count)"
  echo "#define BUILD_DATE \"$(date '+%b %d %Y @ %T')\""
} > "$GH_REPO/common/git_ver.h"
make -C "$GH_REPO/plugin_src/trophy_unlocker_1352" clean
make -C "$GH_REPO/plugin_src/trophy_unlocker_1352"
PRX="$GH_REPO/bin/plugins/prx_final/trophy_unlocker_1352.prx"
test -s "$PRX"
cp "$PRX" installer/trophy_unlocker_1352.prx

# Build, validate and stage the V4 package.
make -C installer clean
make -C installer
PKG="installer/IV0000-BREW13522_00-TROPHYUNLOCK1352.pkg"
test -s "$PKG"
"$TOOL" sfo_listentries installer/sce_sys/param.sfo
"$TOOL" pkg_validate --verbose "$PKG"
mkdir -p dist
cp "$PKG" dist/Trophy_Unlocker_13.52_Manager_V4.pkg
cp "$PRX" dist/trophy_unlocker_1352.prx
sha256sum dist/Trophy_Unlocker_13.52_Manager_V4.pkg dist/trophy_unlocker_1352.prx > dist/SHA256SUMS.txt
ls -lh dist
