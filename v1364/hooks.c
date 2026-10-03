/* V13.6.3-specific, position-independent hooks; no new PS4 imports or syscalls.
 * Addresses are resolved by hooks.ld and guarded by the patcher's full SHA-256.
 * No firmware/kernel offsets. No persistent writable globals. */
typedef unsigned long usize;
typedef long isize;
typedef unsigned int u32;
typedef unsigned char u8;
#define API __attribute__((visibility("hidden")))
extern API int ps_open(const char *, int, int);
extern API int ps_close(int);
extern API isize ps_write(int, const void *, usize);
extern API int ps_fsync(int);
extern API int ps_mkdir(const char *, int);
extern API int ps_load(const char *, usize, const void *, u32, void *, void *);
extern API int ps_dlsym(int, const char *, void **);
extern API int *tu_errno(void);
extern API int tu_ioctl(int, unsigned long, ...);
extern API int original_load_api(void);
extern API int original_decrypt(const void *, u8 *);
extern API void *mount_init_ptr, *mount_ptr, *unmount_init_ptr, *unmount_ptr;

const char external_root[] = "/data/TrophyUnlocker1352/metadata";
static const char log_path[] = "/data/TrophyUnlocker1352/v13_6_4-uninstalled.log";

/* Per-attempt log: reset at the start, at most a fixed number of short lines.
 * Open/fsync/close each line so a process crash still leaves the last stage.
 * No account identifiers, keys or metadata contents are written. */
void log_stage(const char *stage, int code, int reset) {
    int *ep = tu_errno();
    int saved = *ep;
    char line[96];
    usize n = 0;
    while (*stage && n < 72) line[n++] = *stage++;
    line[n++] = ' '; line[n++] = '0'; line[n++] = 'x';
    const char *hex = "0123456789abcdef";
    for (int shift = 28; shift >= 0; shift -= 4)
        line[n++] = hex[((u32)code >> shift) & 15];
    line[n++] = '\n';
    /* FreeBSD/PS4 flags: WRONLY=1, CREAT=0x200, TRUNC=0x400, APPEND=8. */
    int fd = ps_open(log_path, 0x201 | (reset ? 0x400 : 8), 0600);
    if (fd >= 0) {
        usize pos = 0;
        /* Bounded even if a filesystem repeatedly returns short writes. */
        for (int tries = 0; pos < n && tries < 8; ++tries) {
            isize w = ps_write(fd, line + pos, n - pos);
            if (w <= 0 || (usize)w > n - pos) break;
            pos += (usize)w;
        }
        ps_fsync(fd);
        ps_close(fd);
    }
    *ep = saved;
}

void scan_begin(void) {
    ps_mkdir("/data/TrophyUnlocker1352", 0755);
    ps_mkdir(external_root, 0755);
    log_stage("V13.6.4 TEST: metadata scan begin", 0, 1);
}

void native_begin(void) {
    log_stage("metadata found: native unlock begin", 0, 0);
}

int mount_preflight(void) {
    /* Mirrors libkernel_sys/statfs resolution in PS4-vsh-utils loadTrophyLib.
     * This is compatibility preparation, NOT a proven CE-34878-0 fix. */
    /* In this hash-pinned executable the only loader callsite is this hook.
     * Reuse its completed initialization across individual trophy attempts. */
    if (mount_init_ptr && mount_ptr && unmount_init_ptr && unmount_ptr) {
        log_stage("TrophyData API already initialized", 0, 0);
        return 0;
    }
    log_stage("metadata unavailable: kernel_sys load begin", 0, 0);
    int handle = ps_load("/system/common/lib/libkernel_sys.sprx", 0, 0, 0, 0, 0);
    log_stage("kernel_sys load end", handle, 0);
    if (handle < 0) return handle;
    void *statfs = 0;
    int rc = ps_dlsym(handle, "statfs", &statfs);
    log_stage("statfs resolve end", rc, 0);
    if (rc < 0) return rc;
    if (!statfs) return -13641;
    rc = original_load_api();
    log_stage("TrophyData API load end", rc, 0);
    if (rc < 0) return rc;
    if (!mount_init_ptr || !mount_ptr || !unmount_init_ptr || !unmount_ptr) {
        log_stage("TrophyData missing function pointer", -13642, 0);
        return -13642;
    }
    return 0;
}

int decrypt_logged(const void *path, u8 *out) {
    log_stage("sealedkey decrypt begin", 0, 0);
    int rc = original_decrypt(path, out);
    log_stage("sealedkey decrypt end", rc, 0);
    return rc;
}

int ioctl_padded(int fd, unsigned long request, void *buffer) {
    /* _IOWR's encoded payload length is 0x84, not 0x80. Keep the additional
     * bytes off the original caller's neighboring stack locals. */
    u8 scratch[0x90];
    if (request != 0xc0845302UL || !buffer) return tu_ioctl(fd, request, buffer);
    for (usize i = 0; i < sizeof scratch; ++i) scratch[i] = 0;
    for (usize i = 0; i < 0x80; ++i) scratch[i] = ((u8 *)buffer)[i];
    int rc = tu_ioctl(fd, request, scratch);
    if (rc >= 0)
        for (usize i = 0; i < 0x80; ++i) ((u8 *)buffer)[i] = scratch[i];
    /* Clear the temporary key buffer without an optimizable memset. */
    for (usize i = 0; i < sizeof scratch; ++i) ((volatile u8 *)scratch)[i] = 0;
    return rc;
}

typedef int (*MountFn)(void *, const char *, const char *, const u8 *);
int mount_logged(void *opt, const char *volume, const char *path,
                 const u8 *key, MountFn fn) {
    log_stage("trophy.img mount begin", 0, 0);
    int rc = fn ? fn(opt, volume, path, key) : -13642;
    log_stage("trophy.img mount end", rc, 0);
    return rc;
}

typedef int (*UnmountFn)(void *, const char *, int, int);
int unmount_logged(void *opt, const char *path, int handle, int ignore,
                   UnmountFn fn) {
    log_stage("trophy.img scan finished: unmount begin", 0, 0);
    int rc = fn ? fn(opt, path, handle, ignore) : -13642;
    log_stage("trophy.img unmount end", rc, 0);
    return rc;
}
