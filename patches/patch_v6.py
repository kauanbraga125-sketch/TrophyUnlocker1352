from pathlib import Path

p = Path('installer/src/main.c')
s = p.read_text()

# V6 intentionally avoids sceVideoOut/direct-memory framebuffer code.
needle = '#include "sfo_title.h"\n'
extra = '#include <orbis/CommonDialog.h>\n#include <orbis/MsgDialog.h>\n#include <orbis/Sysmodule.h>\n'
if extra not in s:
    if needle not in s:
        raise SystemExit('sfo_title include not found')
    s = s.replace(needle, needle + extra, 1)

notify_marker = 'static void notify(const char *msg) {\n'
if notify_marker not in s:
    raise SystemExit('notify function not found')

helper = r'''static int g_dialog_ready = 0;

static inline void tuCommonDialogSetMagicNumber(uint32_t *magic, const OrbisCommonDialogBaseParam *param) {
    *magic = (uint32_t)(ORBIS_COMMON_DIALOG_MAGIC_NUMBER + (uint64_t)param);
}

static inline void tuCommonDialogBaseParamInit(OrbisCommonDialogBaseParam *param) {
    memset(param, 0, sizeof(*param));
    param->size = (uint32_t)sizeof(*param);
    tuCommonDialogSetMagicNumber(&param->magic, param);
}

static inline void tuMsgDialogParamInit(OrbisMsgDialogParam *param) {
    memset(param, 0, sizeof(*param));
    tuCommonDialogBaseParamInit(&param->baseParam);
    param->size = sizeof(*param);
}

static int dialog_ui_init(void) {
    if (g_dialog_ready) return 0;
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG) < 0) return -1;
    if (sceCommonDialogInitialize() < 0) return -2;
    g_dialog_ready = 1;
    return 0;
}

static void dialog_ui_show(const char *msg) {
    if (!g_dialog_ready || !msg || !*msg) return;

    OrbisMsgDialogParam param;
    OrbisMsgDialogUserMessageParam user;
    OrbisMsgDialogResult result;

    if (sceMsgDialogInitialize() < 0) return;
    tuMsgDialogParamInit(&param);
    memset(&user, 0, sizeof(user));
    user.msg = msg;
    user.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_OK;
    param.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;
    param.userMsgParam = &user;

    if (sceMsgDialogOpen(&param) >= 0) {
        while (sceMsgDialogUpdateStatus() != ORBIS_COMMON_DIALOG_STATUS_FINISHED)
            sceKernelUsleep(10000);
        memset(&result, 0, sizeof(result));
        (void)sceMsgDialogGetResult(&result);
        (void)sceMsgDialogClose();
    }
    (void)sceMsgDialogTerminate();
}

'''
s = s.replace(notify_marker, helper + notify_marker, 1)

# Make every existing status notification visible using Sony's native dialog UI.
needle2 = '    sceKernelDebugOutText(0, "%s\\n", msg);\n'
if needle2 not in s:
    raise SystemExit('notify debug line not found')
s = s.replace(needle2, needle2 + '    dialog_ui_show(msg);\n', 1)

old = 'int main(void) {\n    init_notify();\n'
new = 'int main(void) {\n    init_notify();\n    (void)dialog_ui_init();\n'
if old not in s:
    raise SystemExit('main init block not found')
s = s.replace(old, new, 1)

s = s.replace('Trophy Unlocker 13.52 V4: preparando plugin GoldHEN...',
              'Trophy Unlocker 13.52 V6: iniciando modo seguro de interface...', 1)

if 'sceVideoOut' in s or 'ui_video' in s:
    raise SystemExit('V6 must not contain VideoOut framebuffer code')
if 'dialog_ui_show(msg);' not in s:
    raise SystemExit('dialog UI not wired into notify')

p.write_text(s)
print('V6 patch applied: native MsgDialog UI, no direct video memory')
