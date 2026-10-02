#include "../goldhen_access.h"
#include "../library.h"
#include "fake_fs.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static tu::GoldhenReply version_reply, access_reply;
static unsigned calls;
static tu::GoldhenReply invoke(uint64_t command, void* data) {
    ++calls;
    if (calls == 1) { assert(command == 0 && data == nullptr); return version_reply; }
    assert(calls == 2 && command == 2 && data);
    const unsigned char* bytes=static_cast<unsigned char*>(data);
    for (size_t i=0;i<sizeof(tu::JailbreakBackup);++i) assert(bytes[i] == 0);
    return access_reply;
}
static tu::AccessResult run(tu::GoldhenReply v, tu::GoldhenReply a) {
    version_reply=v; access_reply=a; calls=0;
    tu::JailbreakBackup backup;
    memset(&backup,0xa5,sizeof(backup));
    return tu::query_goldhen_access(invoke,backup);
}
int main() {
    // Exact regression: the V13 wrapper mapped RAX=256, CF=1 to SDK=-256.
    auto result=run({256,1},{0,0});
    assert(calls == 2 && result.sdk == 256 && result.sdk_carry == 1);
    assert(result.jailbreak_attempted && result.acknowledged());
    result=run({256,0},{0,0}); assert(calls == 2 && result.acknowledged());
    result=run({256,1},{0,1}); assert(calls == 2 && result.acknowledged() && result.jailbreak_carry == 1);
    // Unknown, actual negative and BSD error replies must not trigger access.
    for (int64_t value : {-256, -1, 1, 78, 0, 512}) {
        result=run({value,1},{0,0});
        assert(calls == 1 && !result.jailbreak_attempted && !result.acknowledged());
        assert(result.sdk == value); // No abs(), invented ENOSYS, or fake success.
    }
    result=run({256,1},{13,1});
    assert(calls == 2 && result.jailbreak_attempted && result.jailbreak == 13 && !result.acknowledged());
    result=run({256,1},{-13,0}); assert(!result.acknowledged() && result.jailbreak == -13);
    puts("PASS GoldHEN raw reply, carry and guarded command sequence");
    FakeFS fs; tu::Library library(fs); library.scan();
    result=run({256,1},{0,0});
    // An acknowledged access command must not invent discovered games.
    assert(result.acknowledged() && library.games.empty() && library.roots[0].status == -2);
    std::string report=library.diagnostic(result);
    assert(report.find("sdk_raw=256 sdk_CF=1") != std::string::npos);
    assert(report.find("access_sent=1 access_raw=0") != std::string::npos);
    result=run({78,1},{0,0});
    assert(library.diagnostic(result).find("access_sent=0") != std::string::npos);
    puts("PASS access diagnostic distinguishes uncalled command from real response");
}
