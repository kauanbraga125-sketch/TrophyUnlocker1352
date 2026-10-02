#pragma once
#include "filesystem.h"

namespace tu {
// Preserve the kernel's value and carry separately. SDK VERSION is a private
// protocol response, so a generic errno transform must not destroy its value.
struct GoldhenReply { int64_t value; uint64_t carry; };
struct JailbreakBackup {
    uint32_t uid, ruid, rgid, groups;
    uint64_t paid, caps[2];
    void *prison, *cdir, *jdir, *rdir;
};
static_assert(sizeof(JailbreakBackup) == 72, "GoldHEN SDK backup ABI");
static_assert(offsetof(JailbreakBackup, prison) == 40, "GoldHEN SDK prison offset");
using GoldhenCall = GoldhenReply (*)(uint64_t, void*);
AccessResult query_goldhen_access(GoldhenCall, JailbreakBackup&);
}
