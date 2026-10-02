#!/usr/bin/env bash
set -euxo pipefail

# V11.2 stays on the exact V10 packaging/UI baseline that worked on hardware.
# The only functional addition is a read-only filesystem probe started by X after the UI is already running.
cp ci/build_v10_visual.sh /tmp/build_v11_2.sh

python3 - <<'PY'
from pathlib import Path
p = Path('/tmp/build_v11_2.sh')
s = p.read_text()
s = s.replace('.deps_v10_visual', '.deps_v11_2')
s = s.replace('v10_visual_work', 'v11_2_work')
s = s.replace('dist_v10', 'dist_v11_2')
s = s.replace('Trophy Unlocker 13.52 V10 Visual', 'Trophy Unlocker 13.52 V11.2 Probe')
s = s.replace('Trophy Unlocker V10', 'Trophy Unlocker V11.2')
s = s.replace('V10 VISUAL SMOKE - INTERFACE SEGURA', 'V11.2 SAFE - X PARA TESTAR ACESSO')
s = s.replace('BREW13528', 'BREW13531')
s = s.replace('TROPHYVISUALV100', 'TROPHYPROBEV1120')
s = s.replace('Trophy_Unlocker_13.52_V10_VISUAL.pkg', 'Trophy_Unlocker_13.52_V11_2_PROBE.pkg')

s = s.replace('#include <string.h>\n', '#include <string.h>\n#include <dirent.h>\n#include <errno.h>\n#include <ctype.h>\n')

helper = r'''
static int is_cusa_id(const char* name)
{
    if (!name || strlen(name) != 9 || strncmp(name, "CUSA", 4) != 0) return 0;
    for (int i = 4; i < 9; ++i) if (!isdigit((unsigned char)name[i])) return 0;
    return 1;
}

static int scan_root(const char* root, char* first, size_t first_sz)
{
    DIR* d = opendir(root);
    if (!d) return -errno;
    int count = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (!is_cusa_id(ent->d_name)) continue;
        ++count;
        if (first && first_sz && first[0] == '\0') snprintf(first, first_sz, "%s", ent->d_name);
    }
    closedir(d);
    return count;
}

static int probe_dir(const char* path)
{
    DIR* d = opendir(path);
    if (!d) return -errno;
    closedir(d);
    return 0;
}
'''
s = s.replace('int main(void)\n{', helper + '\nint main(void)\n{')

s = s.replace('SDL_Texture* notice = make_text(r, "CAPAS DE TESTE - NENHUM PRX E NENHUM TROFEU ALTERADO", g_body);',
'''SDL_Texture* notice = make_text(r, "X: TESTAR /USER/APP     NENHUM PRX E NENHUM TROFEU ALTERADO", g_body);\n    SDL_Texture* probeText = make_text(r, "PROBE: AINDA NAO EXECUTADO", g_body);\n    int probe_done = 0;''')

s = s.replace('else if (ev.jbutton.button == 0) { details = 1; }',
'''else if (ev.jbutton.button == 0) {\n                    if (!probe_done) {\n                        char first[32]; memset(first, 0, sizeof(first));\n                        int a = scan_root("/user/app", first, sizeof(first));\n                        int e = scan_root("/mnt/ext0/user/app", first, sizeof(first));\n                        int m = probe_dir("/user/appmeta");\n                        int d = probe_dir("/data");\n                        char line[512];\n                        snprintf(line, sizeof(line), "PROBE: /user/app=%d   ext0=%d   appmeta=%d   /data=%d   primeiro=%s", a, e, m, d, first[0] ? first : "nenhum");\n                        if (probeText) SDL_DestroyTexture(probeText);\n                        probeText = make_text(r, line, g_body);\n                        probe_done = 1;\n                    }\n                    details = 1;\n                }''')

s = s.replace('draw_tex(r, notice, 90, 1030, 1500);', 'draw_tex(r, probeText, 90, 945, 1700);\n        draw_tex(r, notice, 90, 1030, 1500);')

# Safety assertions: exact V10 launch model must remain unchanged.
assert "CATEGORY --type Utf8 --maxsize 4 --value 'gd'" in s
assert 'ATTRIBUTE --type Integer --maxsize 4 --value 0' in s
assert '--paid 0x3800000000000011' in s
assert '--authinfo' not in s
assert 'scan_root("/user/app"' in s
p.write_text(s)
PY

chmod +x /tmp/build_v11_2.sh
/tmp/build_v11_2.sh
