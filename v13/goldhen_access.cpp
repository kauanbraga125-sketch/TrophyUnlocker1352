#include "goldhen_access.h"
#include <cstring>

namespace tu {
AccessResult query_goldhen_access(GoldhenCall command, JailbreakBackup& backup) {
    AccessResult result;
    result.attempted = true;
    GoldhenReply version = command(0, nullptr);
    result.sdk = version.value;
    result.sdk_carry = version.carry;
    // Only the documented VERSION signature (0x100) authorizes command 2.
    // V13 negated this value when CF=1, then incorrectly rejected -256.
    // Do not accept arbitrary negative values or replace them with abs().
    if (version.value != 0x100) return result;
    memset(&backup, 0, sizeof(backup));
    result.jailbreak_attempted = true;
    GoldhenReply access = command(2, &backup);
    result.jailbreak = access.value;
    result.jailbreak_carry = access.carry;
    // A zero command response is acknowledged, not proof that paths are visible.
    // The caller must rescan the real directories and report their results.
    return result;
}
}
