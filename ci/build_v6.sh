#!/usr/bin/env bash
set -euxo pipefail

cp ci/build_v4.sh /tmp/build_v6_core.sh

# Start from the stable V4 runtime path, then add only the native PS4 dialog UI.
sed -i '/python3 patches\/patch_v4.py/a python3 patches/patch_v6.py' /tmp/build_v6_core.sh
sed -i 's/Manager V4/Manager V6/g' /tmp/build_v6_core.sh
sed -i 's/BREW13522/BREW13524/g' /tmp/build_v6_core.sh
sed -i 's/Manager_V4\.pkg/Manager_V6.pkg/g' /tmp/build_v6_core.sh

# Use the official OpenOrbis message-dialog libraries. No SceVideoOut/direct framebuffer.
sed -i 's/-lSceUserService$/-lSceUserService -lSceMsgDialog -lSceCommonDialog -lSceSysmodule/' /tmp/build_v6_core.sh

bash /tmp/build_v6_core.sh

# Guard against accidentally reintroducing the V5 direct-video path.
! grep -R -q 'sceVideoOut' installer/src
! grep -R -q 'sceKernelAllocateDirectMemory' installer/src
grep -q 'sceMsgDialogOpen' installer/src/main.c
grep -q 'sceCommonDialogInitialize' installer/src/main.c
test -s dist/Trophy_Unlocker_13.52_Manager_V6.pkg
