#include "filesystem.h"
#include <cerrno>
#include <cstring>

namespace tu {
bool valid_path(const std::string& p) {
    if (p.empty() || p[0] != '/' || p.size() > 768) return false;
    for (unsigned char c : p) if (c < 32 || c == 127) return false;
    size_t at = 1;
    while (at < p.size()) {
        size_t end = p.find('/', at);
        std::string part = p.substr(at, end == std::string::npos ? end : end-at);
        if (part == ".." || part == ".") return false;
        if (end == std::string::npos) break;
        at = end+1;
    }
    return true;
}
std::string join_path(const std::string& p, const std::string& name) {
    if (name.empty() || name.find('/') != std::string::npos) return "";
    std::string result = p + (p == "/" ? "" : "/") + name;
    return valid_path(result) ? result : "";
}
std::string parent_path(const std::string& p) {
    size_t at = p.find_last_of('/');
    return at == 0 || at == std::string::npos ? "/" : p.substr(0, at);
}
int parse_dirents(const uint8_t* bytes, size_t size, std::vector<DirEntry>& out) {
    size_t pos = 0;
    // Reject the whole batch on malformed input: don't expose a partial valid prefix.
    std::vector<DirEntry> parsed;
    while (pos < size) {
        if (size-pos < 8) return -EIO;
        uint32_t ino; uint16_t rec;
        memcpy(&ino, bytes+pos, 4); memcpy(&rec, bytes+pos+4, 2);
        uint8_t type = bytes[pos+6], len = bytes[pos+7];
        if (rec < 9 || rec > size-pos || len == 0 || size_t(len)+9 > rec) return -EIO;
        const char* name = reinterpret_cast<const char*>(bytes+pos+8);
        if (name[len] != 0 || memchr(name, 0, len) || memchr(name, '/', len)) return -EIO;
        if (ino && strcmp(name, ".") && strcmp(name, "..")) {
            if (out.size()+parsed.size() >= 4096) return -EOVERFLOW;
            parsed.push_back({std::string(name, len), type});
        }
        pos += rec;
    }
    out.insert(out.end(), parsed.begin(), parsed.end());
    return 0;
}
}
