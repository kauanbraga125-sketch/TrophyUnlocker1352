#include "filesystem.h"
#include "goldhen_access.h"
#include <orbis/libkernel.h>
#include <fcntl.h>
#include <cerrno>
#include <algorithm>

namespace tu {
static int native_error(int64_t result) {
    uint32_t bits = uint32_t(result);
    if ((bits & 0xffff0000u) == 0x80020000u) return -int(bits & 0xffffu);
    return result < 0 ? int(result) : 0;
}
class NativeFS : public FileSystem {
public:
    int list(const std::string& path, std::vector<DirEntry>& out) override {
        out.clear();
        if (!valid_path(path)) return -EINVAL;
        int fd = sceKernelOpen(path.c_str(), O_RDONLY | O_DIRECTORY, 0);
        if (fd < 0) return native_error(fd);
        char bytes[16384]; int status = 0;
        for (;;) {
            int n = sceKernelGetdents(fd, bytes, sizeof(bytes));
            if (native_error(n) == -EINTR) continue;
            if (n < 0) { status = native_error(n); break; }
            if (!n) break;
            if (n > int(sizeof(bytes))) { status = -EIO; break; }
            status = parse_dirents(reinterpret_cast<uint8_t*>(bytes), size_t(n), out);
            if (status) break;
        }
        int closed = native_error(sceKernelClose(fd));
        if (!status) status = closed;
        if (status) out.clear();
        return status;
    }
    int read(const std::string& path, size_t limit, std::vector<uint8_t>& out) override {
        out.clear();
        if (!valid_path(path) || limit > 4*1024*1024) return -EINVAL;
        int fd = sceKernelOpen(path.c_str(), O_RDONLY, 0);
        if (fd < 0) return native_error(fd);
        uint8_t chunk[8192]; int status = 0;
        for (;;) {
            size_t request = std::min(sizeof(chunk), limit+1-out.size());
            int64_t n = int64_t(sceKernelRead(fd, chunk, request));
            int err = native_error(n);
            if (err == -EINTR) continue;
            if (err) { status = err; break; }
            if (uint64_t(n) > request) { status = -EIO; break; }
            if (!n) break;
            out.insert(out.end(), chunk, chunk+n);
            if (out.size() > limit) { status = -EFBIG; break; }
        }
        int closed = native_error(sceKernelClose(fd));
        if (!status) status = closed;
        if (status) out.clear();
        return status;
    }
    bool exists(const std::string& path) override {
        if (!valid_path(path)) return false;
        int fd = sceKernelOpen(path.c_str(), O_RDONLY, 0);
        if (fd < 0) return false;
        sceKernelClose(fd); return true;
    }
    int save(const std::string& path, const std::string& contents) override {
        // Only the app's own catalog/log/cache/link/state files are writable. Never games, app.db or trophy databases.
        if (path != "/data/TrophyUnlocker1352/manual-games.txt" &&
            path != "/data/TrophyUnlocker1352/trophy-links.txt" &&
            path != "/data/TrophyUnlocker1352/v13-diagnostic.txt" &&
            path != "/data/TrophyUnlocker1352/library-cache-v1.txt" &&
            path != "/data/TrophyUnlocker1352/ui-state-v1.txt") return -EACCES;
        int mk = native_error(sceKernelMkdir("/data/TrophyUnlocker1352", 0755));
        if (mk && mk != -EEXIST) return mk;
        std::string temp = path+".tmp";
        int fd = sceKernelOpen(temp.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if (fd < 0) return native_error(fd);
        size_t pos = 0; int status = 0;
        while (pos < contents.size()) {
            int64_t n = int64_t(sceKernelWrite(fd, contents.data()+pos, contents.size()-pos));
            int err = native_error(n);
            if (err == -EINTR) continue;
            if (err) { status = err; break; }
            if (n == 0 || uint64_t(n) > contents.size()-pos) { status = -EIO; break; }
            pos += size_t(n);
        }
        if (!status) status = native_error(sceKernelFsync(fd));
        int closed = native_error(sceKernelClose(fd));
        if (!status) status = closed;
        if (!status) status = native_error(sceKernelRename(temp.c_str(), path.c_str()));
        if (status) sceKernelUnlink(temp.c_str());
        return status;
    }
};
FileSystem& native_filesystem() { static NativeFS fs; return fs; }

// GoldHEN_Plugins_SDK (MIT) syscall-0 gateway. Return RAX unchanged and
// capture CF immediately in RDX (the second word of the SysV result struct).
// This is specific to the private GoldHEN protocol; native file errno handling
// above remains unchanged.
extern "C" GoldhenReply tu_goldhen_command(uint64_t command, void* data);
asm(".text\n"
    ".global tu_goldhen_command\n"
    "tu_goldhen_command:\n"
    "mov %rsi, %rdx\n"
    "mov %rdi, %rsi\n"
    "mov $500, %rdi\n"
    "xor %eax, %eax\n"
    "mov %rcx, %r10\n"
    "syscall\n"
    "setc %dl\n"
    "movzbl %dl, %edx\n"
    "ret\n");
AccessResult request_goldhen_access() {
    static AccessResult result;
    static JailbreakBackup backup = {};
    if (result.acknowledged()) return result;
    // Failed or unsupported replies can be retried with TRIANGLE; don't cache
    // a rejection forever. Preserve the backup after an acknowledged request.
    result = query_goldhen_access(tu_goldhen_command, backup);
    return result;
}
}
