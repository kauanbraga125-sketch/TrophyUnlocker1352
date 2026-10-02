#!/usr/bin/env bash
set -euxo pipefail

ROOT="$PWD"
DEPS="$ROOT/.deps_v8"
WORK="$ROOT/v8_work"
DIST="$ROOT/dist_v8"
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

# Start from the same official OpenOrbis dialogs sample that already opened on FW 13.52.
cp -a "$OO_PS4_TOOLCHAIN/samples/dialogs" "$WORK/dialogs"
cd "$WORK/dialogs"

cat > dialogs/main.c <<'EOF'
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>

#include <orbis/CommonDialog.h>
#include <orbis/MsgDialog.h>
#include <orbis/Sysmodule.h>

#define MDIALOG_OK       0
#define MDIALOG_YESNO    1

static inline void _orbisCommonDialogSetMagicNumber(uint32_t* magic, const OrbisCommonDialogBaseParam* param)
{
    *magic = (uint32_t)(ORBIS_COMMON_DIALOG_MAGIC_NUMBER + (uint64_t)param);
}

static inline void _orbisCommonDialogBaseParamInit(OrbisCommonDialogBaseParam *param)
{
    memset(param, 0x0, sizeof(OrbisCommonDialogBaseParam));
    param->size = (uint32_t)sizeof(OrbisCommonDialogBaseParam);
    _orbisCommonDialogSetMagicNumber(&(param->magic), param);
}

static inline void orbisMsgDialogParamInitialize(OrbisMsgDialogParam *param)
{
    memset(param, 0x0, sizeof(OrbisMsgDialogParam));
    _orbisCommonDialogBaseParamInit(&param->baseParam);
    param->size = sizeof(OrbisMsgDialogParam);
}

static int show_dialog(int dialog_type, const char *format, ...)
{
    OrbisMsgDialogParam param;
    OrbisMsgDialogUserMessageParam userMsgParam;
    OrbisMsgDialogResult result;
    char str[0x800];
    memset(str, 0, sizeof(str));

    va_list opt;
    va_start(opt, format);
    vsnprintf(str, sizeof(str), format, opt);
    va_end(opt);

    if (sceMsgDialogInitialize() < 0)
        return 0;

    orbisMsgDialogParamInitialize(&param);
    param.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;

    memset(&userMsgParam, 0, sizeof(userMsgParam));
    userMsgParam.msg = str;
    userMsgParam.buttonType = (dialog_type ? ORBIS_MSG_DIALOG_BUTTON_TYPE_YESNO_FOCUS_NO : ORBIS_MSG_DIALOG_BUTTON_TYPE_OK);
    param.userMsgParam = &userMsgParam;

    if (sceMsgDialogOpen(&param) < 0) {
        sceMsgDialogTerminate();
        return 0;
    }

    do { } while (sceMsgDialogUpdateStatus() != ORBIS_COMMON_DIALOG_STATUS_FINISHED);
    sceMsgDialogClose();

    memset(&result, 0, sizeof(result));
    sceMsgDialogGetResult(&result);
    sceMsgDialogTerminate();
    return (result.buttonId == ORBIS_MSG_DIALOG_BUTTON_ID_YES);
}

static int is_ps4_title_id(const char *name)
{
    size_t n;
    if (!name) return 0;
    n = strlen(name);
    if (n < 9) return 0;
    return strncmp(name, "CUSA", 4) == 0;
}

static int scan_game_root(const char *root, char *first_id, size_t first_id_size)
{
    DIR *dir = opendir(root);
    if (!dir)
        return -1;

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!is_ps4_title_id(ent->d_name))
            continue;
        ++count;
        if (first_id && first_id_size > 0 && first_id[0] == '\0') {
            snprintf(first_id, first_id_size, "%s", ent->d_name);
        }
    }
    closedir(dir);
    return count;
}

int main(void)
{
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG) < 0 ||
        sceCommonDialogInitialize() < 0)
    {
        for(;;);
    }

    char first_id[32];
    memset(first_id, 0, sizeof(first_id));

    int internal_count = scan_game_root("/user/app", first_id, sizeof(first_id));
    int external_count = scan_game_root("/mnt/ext0/user/app", first_id, sizeof(first_id));

    const char *first = first_id[0] ? first_id : "nenhum detectado";
    show_dialog(MDIALOG_OK,
        "Trophy Unlocker 13.52 - V8 abriu.\n\n"
        "Teste de deteccao de jogos:\n"
        "/user/app: %d\n"
        "/mnt/ext0/user/app: %d\n"
        "Primeiro Title ID: %s\n\n"
        "Nenhum PRX foi carregado e nenhum trofeu foi alterado.",
        internal_count, external_count, first);

    for(;;);
}
EOF

sed -i 's/^TITLE       :=.*/TITLE       := Trophy Unlocker 13.52 V8 Discovery/' Makefile
sed -i 's/^TITLE_ID    :=.*/TITLE_ID    := BREW13526/' Makefile
sed -i 's/^CONTENT_ID  :=.*/CONTENT_ID  := IV0000-BREW13526_00-TROPHYDISCOV8000/' Makefile

grep -q "CATEGORY --type Utf8 --maxsize 4 --value 'gd'" Makefile
! grep -q -- '--authinfo' Makefile
grep -q 'scan_game_root' dialogs/main.c
grep -q 'for(;;);' dialogs/main.c

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
PKG="IV0000-BREW13526_00-TROPHYDISCOV8000.pkg"
test -s "$PKG"
"$TOOL" sfo_listentries sce_sys/param.sfo
"$TOOL" pkg_validate --verbose "$PKG"

cp "$PKG" "$DIST/Trophy_Unlocker_13.52_V8_DISCOVERY.pkg"
sha256sum "$DIST/Trophy_Unlocker_13.52_V8_DISCOVERY.pkg" > "$DIST/SHA256SUMS.txt"
file "$DIST/Trophy_Unlocker_13.52_V8_DISCOVERY.pkg"
ls -lh "$DIST"
