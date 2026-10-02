#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS="$ROOT/.deps_v13"
mkdir -p "$DEPS"
if ! command -v clang++-18 >/dev/null || ! command -v ld.lld-18 >/dev/null || ! command -v rg >/dev/null || ! command -v git >/dev/null || [[ ! -f "$(clang++-18 -print-file-name=libclang_rt.asan-x86_64.a)" ]]; then
  sudo apt-get update -qq
  sudo apt-get install -y --no-install-recommends clang-18 lld-18 llvm-18 libclang-rt-18-dev make curl ca-certificates ripgrep git python3
fi
if [[ ! -f "$DEPS/toolchain-llvm-18.tar.gz" ]]; then
  curl -fLsS --retry 3 https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/v0.5.4/toolchain-llvm-18.tar.gz -o "$DEPS/toolchain-llvm-18.tar.gz"
fi
echo "3c7cd5bb593ca74fa1c13fd59f3938dc0fc07985167f7275063019e63abe4526  $DEPS/toolchain-llvm-18.tar.gz" | sha256sum -c -
if [[ ! -f "$DEPS/OpenOrbis/PS4Toolchain/link.x" ]]; then tar xzf "$DEPS/toolchain-llvm-18.tar.gz" -C "$DEPS"; fi
export OO_PS4_TOOLCHAIN="$DEPS/OpenOrbis/PS4Toolchain"
if [[ ! -f "$DEPS/legacy-ssl/usr/lib/x86_64-linux-gnu/libssl.so.1.1" ]]; then
  curl -fLsS --retry 3 https://archive.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2.24_amd64.deb -o "$DEPS/libssl1.1.deb"
  dpkg-deb -x "$DEPS/libssl1.1.deb" "$DEPS/legacy-ssl"
fi
export LD_LIBRARY_PATH="$DEPS/legacy-ssl/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DOTNET_BUNDLE_EXTRACT_BASE_DIR="$DEPS/dotnet-bundle"
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1

# Apollo uses this PS4 SQLite port for trophy_local.db. Pin the exact revision
# so future upstream changes do not silently change our package.
SQLITE_COMMIT="0e9ef79f42777271964fd019afb0b82c437b29e2"
if [[ ! -d "$DEPS/libSQLite-ps4/.git" ]]; then
  git clone -q https://github.com/bucanero/libSQLite-ps4.git "$DEPS/libSQLite-ps4"
fi
git -C "$DEPS/libSQLite-ps4" fetch -q origin "$SQLITE_COMMIT"
git -C "$DEPS/libSQLite-ps4" checkout -q --detach "$SQLITE_COMMIT"
make -C "$DEPS/libSQLite-ps4" clean >/dev/null || true
make -C "$DEPS/libSQLite-ps4" install

bash "$ROOT/v13/build.sh"
