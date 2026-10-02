#!/usr/bin/env bash
set -euxo pipefail
cp ci/build_v4.sh /tmp/build_v5_core.sh
sed -i '/python3 patches\/patch_v4.py/a python3 patches/patch_v5.py' /tmp/build_v5_core.sh
sed -i 's/Manager V4/Manager V5/g' /tmp/build_v5_core.sh
sed -i 's/BREW13522/BREW13523/g' /tmp/build_v5_core.sh
sed -i 's/Manager_V4\.pkg/Manager_V5.pkg/g' /tmp/build_v5_core.sh
sed -i 's/-lSceUserService$/-lSceUserService -lSceVideoOut/' /tmp/build_v5_core.sh
bash /tmp/build_v5_core.sh
grep -q 'ui_init()' installer/src/main.c
grep -q 'ui_render_message(msg)' installer/src/main.c
grep -q 'sceVideoOutOpen' installer/src/ui_video.c
grep -q 'sceVideoOutRegisterBuffers' installer/src/ui_video.c
grep -q 'sceVideoOutSubmitFlip' installer/src/ui_video.c
test -s dist/Trophy_Unlocker_13.52_Manager_V5.pkg
