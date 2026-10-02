#!/usr/bin/env bash
set -euxo pipefail

# Start from V11.2 and only move the diagnostic action from X to Triangle.
cp ci/build_v11_library.sh /tmp/base_v11_2.sh
python3 - <<'PY'
from pathlib import Path
src = Path('/tmp/base_v11_2.sh').read_text()
src = src.replace('V11.2', 'V11.3')
src = src.replace('v11_2', 'v11_3')
src = src.replace('BREW13531', 'BREW13532')
src = src.replace('TROPHYPROBEV1120', 'TROPHYPROBEV1130')
src = src.replace('Trophy_Unlocker_13.52_V11_2_PROBE.pkg', 'Trophy_Unlocker_13.52_V11_3_PROBE.pkg')
src = src.replace('X PARA TESTAR ACESSO', 'TRIANGULO TESTA ACESSO')
src = src.replace('X: TESTAR /USER/APP', 'TRIANGULO: TESTAR /USER/APP')
old = '''else if (ev.jbutton.button == 0) {\\n                    if (!probe_done) {'''
new = '''else if (ev.jbutton.button == 3) {\\n                    if (!probe_done) {'''
src = src.replace(old, new)
# Restore X as the original Details action.
src = src.replace('                    details = 1;\\n                }\\n                else if (ev.jbutton.button == 1)', '                    details = 1;\\n                }\\n                else if (ev.jbutton.button == 0) { details = 1; }\\n                else if (ev.jbutton.button == 1)')
Path('/tmp/build_v11_3.sh').write_text(src)
PY
chmod +x /tmp/build_v11_3.sh
/tmp/build_v11_3.sh
