#!/usr/bin/env bash
set -euxo pipefail

# V11.1 is deliberately built from the exact V10 visual baseline that worked on hardware.
# Only the app identity/output names change in this step. No gde/authinfo experiment.
cp ci/build_v10_visual.sh /tmp/build_v11_1.sh

python3 - <<'PY'
from pathlib import Path
p = Path('/tmp/build_v11_1.sh')
s = p.read_text()
s = s.replace('.deps_v10_visual', '.deps_v11_1')
s = s.replace('v10_visual_work', 'v11_1_work')
s = s.replace('dist_v10', 'dist_v11_1')
s = s.replace('Trophy Unlocker 13.52 V10 Visual', 'Trophy Unlocker 13.52 V11.1 Safe')
s = s.replace('Trophy Unlocker V10', 'Trophy Unlocker V11.1')
s = s.replace('V10 VISUAL SMOKE - INTERFACE SEGURA', 'V11.1 SAFE - BASE V10 ESTAVEL')
s = s.replace('BREW13528', 'BREW13530')
s = s.replace('TROPHYVISUALV100', 'TROPHYSAFEV11100')
s = s.replace('Trophy_Unlocker_13.52_V10_VISUAL.pkg', 'Trophy_Unlocker_13.52_V11_1_SAFE.pkg')

# Safety assertions: preserve the exact packaging model known to launch.
assert "CATEGORY --type Utf8 --maxsize 4 --value 'gd'" in s
assert 'ATTRIBUTE --type Integer --maxsize 4 --value 0' in s
assert '--paid 0x3800000000000011' in s
assert '--authinfo' not in s
p.write_text(s)
PY

chmod +x /tmp/build_v11_1.sh
/tmp/build_v11_1.sh
