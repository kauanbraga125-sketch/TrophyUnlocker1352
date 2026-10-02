#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tu {
struct DirEntry { std::string name; uint8_t type; };
class FileSystem {
public:
    virtual ~FileSystem() {}
    virtual int list(const std::string&, std::vector<DirEntry>&) = 0;
    virtual int read(const std::string&, size_t, std::vector<uint8_t>&) = 0;
    virtual bool exists(const std::string&) = 0;
    virtual int save(const std::string&, const std::string&) = 0;
};
// PS4/FreeBSD directory wire format, not the host's struct dirent.
int parse_dirents(const uint8_t*, size_t, std::vector<DirEntry>&);
std::string join_path(const std::string&, const std::string&);
std::string parent_path(const std::string&);
bool valid_path(const std::string&);
FileSystem& native_filesystem();
struct AccessResult {
    int64_t sdk;
    int64_t jailbreak;
    bool attempted;
    AccessResult() : sdk(0), jailbreak(0), attempted(false) {}
};
AccessResult request_goldhen_access();
}
