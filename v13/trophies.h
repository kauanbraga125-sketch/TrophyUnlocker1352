#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace tu {

struct Trophy {
    int id = 0;
    int group = 0;
    int grade = 0;
    bool unlocked = false;
    std::string title;
    std::string description;
};

struct TrophySet {
    long long database_id = -1;
    std::string np_communication_id;
    std::string title;
    std::vector<Trophy> trophies;
};

struct TrophyLoadResult {
    int status = -1;
    int user_status = -1;
    int sqlite_status = -1;
    uint32_t user_id = 0;
    std::string database_path;
    std::string detail;
    TrophySet set;
};

bool valid_np_communication_id(const std::string& value);
const char* trophy_grade_name(int grade);

// Read-only query helper. Exposed separately so the SQL/schema logic can be
// host-tested without a PS4.
int query_trophy_database(const std::string& database_path,
                          const std::string& np_communication_id,
                          TrophySet& out,
                          std::string& detail);

// PS4 entry point: resolves the active local user and reads that user's
// trophy_local.db. This function never writes to the trophy database.
TrophyLoadResult load_trophies(const std::string& np_communication_id);

}
