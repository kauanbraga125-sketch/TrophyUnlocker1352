#include "trophy_mount.h"
#include "filesystem.h"

#ifndef TU_HOST_TEST
#include <orbis/libkernel.h>
#include <libjbc.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace tu {

#ifndef TU_HOST_TEST
namespace {

static const size_t kEncryptedSealedKeySize = 0x60;
static const size_t kDecryptedSealedKeySize = 0x20;
static const unsigned long kDecryptSealedKeyIoctl = 0xC0845302UL;

struct MountTrophyDataOpt {
    uint8_t reserved;
    char* budgetid;
};

struct UmountTrophyDataOpt {
    uint8_t dummy;
};

using InitMountFn = int (*)(MountTrophyDataOpt*);
using MountFn = int (*)(MountTrophyDataOpt*, const char*, const char*, uint8_t*);
using InitUmountFn = int (*)(UmountTrophyDataOpt*);
using UmountFn = int (*)(UmountTrophyDataOpt*, const char*, int, bool);

struct TrophyApi {
    int module = -1;
    InitMountFn init_mount = nullptr;
    MountFn mount = nullptr;
    InitUmountFn init_umount = nullptr;
    UmountFn umount = nullptr;
};

static void set_failure(TrophyMountProbe& out, int status,
                        const char* stage, const std::string& detail) {
    out.status = status ? status : -1;
    out.stage = stage;
    out.detail = detail;
}

static int prepare_privileged_credentials() {
    jbc_cred cred;
    memset(&cred, 0, sizeof(cred));
    int rc = jbc_get_cred(&cred);
    if (rc != 0) return rc;
    cred.sonyCred |= 0x4000000000000000ULL;
    cred.sceProcType = 0x3801000000000013ULL;
    rc = jbc_set_cred(&cred);
    if (rc != 0) return rc;
    return setuid(0);
}

static int ensure_device(const char* name) {
    char local[96];
    char source[96];
    snprintf(local, sizeof(local), "/dev/%s", name);
    snprintf(source, sizeof(source), "/tu_rootdev/%s", name);

    int fd = open(local, O_RDWR, 0);
    if (fd >= 0) {
        close(fd);
        return 0;
    }

    struct stat st;
    memset(&st, 0, sizeof(st));
    if (stat(source, &st) != 0) return -errno;

    if (mknod(local, S_IFCHR | 0777, st.st_rdev) != 0 && errno != EEXIST)
        return -errno;

    fd = open(local, O_RDWR, 0);
    if (fd < 0) return -errno;
    close(fd);
    return 0;
}

static int prepare_devices(TrophyMountProbe& out) {
    int rc = jbc_mount_in_sandbox("/dev/", "tu_rootdev");
    if (rc != 0) return rc;

    out.pfsctl_status = ensure_device("pfsctldev");
    out.lvdctl_status = ensure_device("lvdctl");
    out.sbl_status = ensure_device("sbl_srv");

    int unmount_rc = jbc_unmount_in_sandbox("tu_rootdev");
    if (out.pfsctl_status != 0) return out.pfsctl_status;
    if (out.lvdctl_status != 0) return out.lvdctl_status;
    if (out.sbl_status != 0) return out.sbl_status;
    return unmount_rc;
}

static int resolve_trophy_api(TrophyMountProbe& out, TrophyApi& api) {
    int rc = jbc_mount_in_sandbox("/system/priv/lib", "tu_priv");
    if (rc != 0) return rc;

    api.module = sceKernelLoadStartModule("/tu_priv/libSceFsInternalForVsh.sprx",
                                         0, nullptr, 0, nullptr, nullptr);
    int unmount_rc = jbc_unmount_in_sandbox("tu_priv");
    out.module_status = api.module;
    if (api.module < 0) return api.module;
    if (unmount_rc != 0) return unmount_rc;

    out.dlsym_init_mount = sceKernelDlsym(api.module, "sceFsInitMountTrophyDataOpt",
                                          reinterpret_cast<void**>(&api.init_mount));
    out.dlsym_mount = sceKernelDlsym(api.module, "sceFsMountTrophyData",
                                     reinterpret_cast<void**>(&api.mount));
    out.dlsym_init_umount = sceKernelDlsym(api.module, "sceFsInitUmountTrophyDataOpt",
                                           reinterpret_cast<void**>(&api.init_umount));
    out.dlsym_umount = sceKernelDlsym(api.module, "sceFsUmountTrophyData",
                                      reinterpret_cast<void**>(&api.umount));

    if (out.dlsym_init_mount < 0 || !api.init_mount) return out.dlsym_init_mount < 0 ? out.dlsym_init_mount : -ENOENT;
    if (out.dlsym_mount < 0 || !api.mount) return out.dlsym_mount < 0 ? out.dlsym_mount : -ENOENT;
    if (out.dlsym_init_umount < 0 || !api.init_umount) return out.dlsym_init_umount < 0 ? out.dlsym_init_umount : -ENOENT;
    if (out.dlsym_umount < 0 || !api.umount) return out.dlsym_umount < 0 ? out.dlsym_umount : -ENOENT;
    return 0;
}

static int decrypt_sealed_key(const std::string& key_path, uint8_t* decrypted) {
    uint8_t data[kEncryptedSealedKeySize + kDecryptedSealedKeySize];
    memset(data, 0, sizeof(data));

    int key = open(key_path.c_str(), O_RDONLY, 0);
    if (key < 0) return -errno;
    ssize_t got = read(key, data, kEncryptedSealedKeySize);
    int saved_errno = errno;
    close(key);
    if (got != static_cast<ssize_t>(kEncryptedSealedKeySize))
        return got < 0 ? -saved_errno : -EIO;

    int sbl = open("/dev/sbl_srv", O_RDWR, 0);
    if (sbl < 0) return -errno;
    int rc = ioctl(sbl, kDecryptSealedKeyIoctl, data);
    saved_errno = errno;
    close(sbl);
    if (rc < 0) return -saved_errno;

    memcpy(decrypted, data + kEncryptedSealedKeySize, kDecryptedSealedKeySize);
    return 0;
}

static int verify_mount(const std::string& path) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (stat(path.c_str(), &st) != 0) return -errno;
    if (!S_ISDIR(st.st_mode)) return -ENOTDIR;
    int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) return -errno;
    close(fd);
    return 0;
}

} // namespace
#endif

TrophyMountProbe probe_trophy_container(uint32_t user_id,
                                        const std::string& np_communication_id) {
    TrophyMountProbe out;
    out.np_communication_id = np_communication_id;

#ifndef TU_HOST_TEST
    if (!valid_path("/data/TrophyUnlocker1352")) {
        set_failure(out, -EINVAL, "paths", "Diretorio de trabalho invalido.");
        return out;
    }
    if (np_communication_id.size() != 12 ||
        np_communication_id.compare(0, 4, "NPWR") != 0) {
        set_failure(out, -EINVAL, "npwr", "NPWR invalido; montagem cancelada antes de tocar no container.");
        return out;
    }

    char folder[160];
    snprintf(folder, sizeof(folder), "/user/home/%08x/trophy/data/%s",
             unsigned(user_id), np_communication_id.c_str());
    out.folder = folder;
    out.mount_path = "/data/TrophyUnlocker1352/trophy_mount";

    std::string image = out.folder + "/trophy.img";
    std::string key = out.folder + "/sealedkey";
    struct stat st;
    if (stat(image.c_str(), &st) != 0) {
        set_failure(out, -errno, "trophy.img", "trophy.img nao foi encontrado ou nao esta acessivel.");
        return out;
    }
    if (stat(key.c_str(), &st) != 0) {
        set_failure(out, -errno, "sealedkey", "sealedkey nao foi encontrado ou nao esta acessivel.");
        return out;
    }
    out.key_status = 0;

    AccessResult access = request_goldhen_access();
    out.goldhen_status = access.acknowledged() ? 0 : int(access.jailbreak);
    if (!access.acknowledged()) {
        set_failure(out, out.goldhen_status ? out.goldhen_status : -EACCES,
                    "goldhen", "GoldHEN nao confirmou o pedido de acesso privilegiado.");
        return out;
    }

    int rc = prepare_privileged_credentials();
    if (rc != 0) {
        set_failure(out, rc, "credenciais", "Falha ao preparar credenciais VSH/root; montagem nao executada.");
        return out;
    }

    rc = prepare_devices(out);
    if (rc != 0) {
        set_failure(out, rc, "dispositivos", "Falha ao preparar /dev/pfsctldev, /dev/lvdctl ou /dev/sbl_srv.");
        return out;
    }

    TrophyApi api;
    rc = resolve_trophy_api(out, api);
    if (rc != 0) {
        set_failure(out, rc, "libSceFsInternalForVsh", "Biblioteca/simbolos de montagem de trofeus indisponiveis.");
        return out;
    }

    uint8_t decrypted[kDecryptedSealedKeySize];
    memset(decrypted, 0, sizeof(decrypted));
    out.decrypt_status = decrypt_sealed_key(key, decrypted);
    if (out.decrypt_status != 0) {
        set_failure(out, out.decrypt_status, "sealedkey-decrypt", "Nao foi possivel decifrar sealedkey; trophy.img nao foi montado.");
        return out;
    }

    (void)mkdir("/data/TrophyUnlocker1352", 0755);
    (void)rmdir(out.mount_path.c_str());

    MountTrophyDataOpt mount_opt;
    memset(&mount_opt, 0, sizeof(mount_opt));
    out.init_mount_status = api.init_mount(&mount_opt);
    if (out.init_mount_status < 0) {
        set_failure(out, out.init_mount_status, "init-mount", "sceFsInitMountTrophyDataOpt falhou.");
        return out;
    }
    mount_opt.budgetid = const_cast<char*>("system");

    out.mount_status = api.mount(&mount_opt, image.c_str(), out.mount_path.c_str(), decrypted);
    memset(decrypted, 0, sizeof(decrypted));
    if (out.mount_status < 0) {
        set_failure(out, out.mount_status, "mount", "sceFsMountTrophyData recusou o container; app permaneceu ativo.");
        return out;
    }

    out.verify_status = verify_mount(out.mount_path);

    UmountTrophyDataOpt umount_opt;
    memset(&umount_opt, 0, sizeof(umount_opt));
    out.init_umount_status = api.init_umount(&umount_opt);
    if (out.init_umount_status >= 0)
        out.umount_status = api.umount(&umount_opt, out.mount_path.c_str(), 0, false);
    else
        out.umount_status = out.init_umount_status;
    (void)rmdir(out.mount_path.c_str());

    if (out.verify_status != 0) {
        set_failure(out, out.verify_status, "verify", "A API informou montagem, mas o ponto montado nao ficou legivel.");
        return out;
    }
    if (out.umount_status < 0) {
        set_failure(out, out.umount_status, "umount", "Container montou, mas a desmontagem retornou erro.");
        return out;
    }

    out.status = 0;
    out.stage = "ok";
    out.detail = "trophy.img montou, ficou legivel e foi desmontado sem alterar trofeus.";
#else
    (void)user_id;
    out.status = -1;
    out.stage = "host";
    out.detail = "Probe disponivel apenas no PS4.";
#endif
    return out;
}

} // namespace tu
