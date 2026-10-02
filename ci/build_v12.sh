#!/usr/bin/env bash
set -euxo pipefail

cp ci/build_v10_visual.sh /tmp/build_v12.sh

python3 - <<'PY'
from pathlib import Path
import re

p = Path('/tmp/build_v12.sh')
s = p.read_text()
main = Path('v12/main.cpp').read_text().rstrip()

pattern = re.compile(r"cat > SDL2/main\.cpp <<'EOF'\n.*?\nEOF\n\ncat > Makefile <<'EOF'", re.S)
replacement = "cat > SDL2/main.cpp <<'EOF'\n" + main + "\nEOF\n\ncat > Makefile <<'EOF'"
s, count = pattern.subn(replacement, s, count=1)
assert count == 1

s = s.replace('.deps_v10_visual', '.deps_v12')
s = s.replace('v10_visual_work', 'v12_work')
s = s.replace('dist_v10', 'dist_v12')
s = s.replace('Trophy Unlocker 13.52 V10 Visual', 'Trophy Unlocker 13.52 V12 UI')
s = s.replace('BREW13528', 'BREW13533')
s = s.replace('TROPHYVISUALV100', 'TROPHYV12UI01200')
s = s.replace('Trophy_Unlocker_13.52_V10_VISUAL.pkg', 'Trophy_Unlocker_13.52_V12_UI.pkg')

assert "CATEGORY --type Utf8 --maxsize 4 --value 'gd'" in s
assert 'ATTRIBUTE --type Integer --maxsize 4 --value 0' in s
assert '--paid 0x3800000000000011' in s
assert '--authinfo' not in s
assert 'V12 - INTERFACE ORGANIZADA' in s

p.write_text(s)
PY

chmod +x /tmp/build_v12.sh
/tmp/build_v12.sh
