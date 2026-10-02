#!/usr/bin/env bash
set -euxo pipefail

cp ci/build_v11_library.sh /tmp/v11_3_gen.sh
python3 - <<'PY'
from pathlib import Path
p=Path('/tmp/v11_3_gen.sh')
s=p.read_text()
s=s.replace('V11.2','V11.3')
s=s.replace('v11_2','v11_3')
s=s.replace('BREW13531','BREW13532')
s=s.replace('TROPHYPROBEV1120','TROPHYPROBEV1130')
s=s.replace('V11_2_PROBE','V11_3_PROBE')
s=s.replace('X PARA TESTAR ACESSO','TRIANGULO TESTA ACESSO')
s=s.replace('X: TESTAR /USER/APP','TRIANGULO: TESTAR /USER/APP')
needle='else if (ev.jbutton.button == 0) {\\n                    if (!probe_done) {'
s=s.replace(needle,'else if (ev.jbutton.button == 3) {\\n                    if (!probe_done) {')
s=s.replace('else if (ev.jbutton.button == 1) { details = 0; }','else if (ev.jbutton.button == 0) { details = 1; }\\n                else if (ev.jbutton.button == 1) { details = 0; }')
p.write_text(s)
PY
chmod +x /tmp/v11_3_gen.sh
/tmp/v11_3_gen.sh
