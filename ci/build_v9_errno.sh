#!/usr/bin/env bash
set -euxo pipefail

cp ci/build_v8_discovery.sh /tmp/build_v9.sh

python3 - <<'PY'
from pathlib import Path
p = Path('/tmp/build_v9.sh')
s = p.read_text()
s = s.replace('.deps_v8', '.deps_v9')
s = s.replace('v8_work', 'v9_work')
s = s.replace('dist_v8', 'dist_v9')
s = s.replace('V8 Discovery', 'V9 Errno')
s = s.replace('V8 abriu.', 'V9 abriu.')
s = s.replace('BREW13526', 'BREW13527')
s = s.replace('TROPHYDISCOV8000', 'TROPHYERRNOV9000')
s = s.replace('V8_DISCOVERY', 'V9_ERRNO')
s = s.replace('#include <dirent.h>', '#include <dirent.h>\n#include <errno.h>')
s = s.replace('if (!dir)\n        return -1;', 'if (!dir)\n        return -errno;')
s = s.replace('"Teste de deteccao de jogos:\\n"', '"Teste de deteccao (negativo = -errno):\\n"')
p.write_text(s)
PY

chmod +x /tmp/build_v9.sh
/tmp/build_v9.sh
