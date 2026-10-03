#pragma once
#include <cstdint>
#include <string>

namespace tu {

struct TrophyMountProbe {
    int status = -1;
    int goldhen_status = -1;
    int module_status = -1;
    int dlsym_init_mount = -1;
    int dlsym_mount = -1;
    int dlsym_init_umount = -1;
    int dlsym_umount = -1;
    int pfsctl_status = -1;
    int lvdctl_status = -1;
    int sbl_status = -1;
    int key_status = -1;
    int decrypt_status = -1;
    int init_mount_status = -1;
    int mount_status = -1;
    int verify_status = -1;
    int init_umount_status = -1;
    int umount_status = -1;
    std::string np_communication_id;
    std::string folder;
    std::string mount_path;
    std::string stage;
    std::string detail;
};

// Read-only/safe preflight: prepares GoldHEN access, resolves the trophy-specific
// VSH filesystem API, decrypts the existing sealed key, mounts trophy.img, checks
// the mount point, and immediately unmounts it. It never changes trophy flags.
TrophyMountProbe probe_trophy_container(uint32_t user_id,
                                        const std::string& np_communication_id);

}
