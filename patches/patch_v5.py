from pathlib import Path
p = Path('installer/src/main.c')
s = p.read_text()
needle = '#include "sfo_title.h"\n'
if '#include "ui_video.h"' not in s:
    if needle not in s:
        raise SystemExit('sfo_title include not found')
    s = s.replace(needle, needle + '#include "ui_video.h"\n', 1)
if 'ui_render_message(msg);' not in s:
    marker = '    if (g_notify) {\n'
    if marker not in s:
        raise SystemExit('notify marker not found')
    s = s.replace(marker, '    ui_render_message(msg);\n' + marker, 1)
old = 'int main(void) {\n    init_notify();\n'
new = '''int main(void) {\n    init_notify();\n    int ui_rc = ui_init();\n    if (ui_rc < 0) {\n        char ui_msg[128];\n        snprintf(ui_msg, sizeof(ui_msg), "UI de video falhou (%d); usando notificacoes/log.", ui_rc);\n        notify(ui_msg);\n    }\n'''
if old not in s:
    raise SystemExit('main init block not found')
s = s.replace(old, new, 1)
s = s.replace('Trophy Unlocker 13.52 V4: preparando plugin GoldHEN...',
              'Trophy Unlocker 13.52 V5: preparando plugin GoldHEN...', 1)
p.write_text(s)
print('V5 patch applied: framebuffer UI enabled')
