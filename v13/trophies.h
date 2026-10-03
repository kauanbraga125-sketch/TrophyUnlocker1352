#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace tu {

class FileSystem;
struct Game;

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

struct TrophyCatalogResult {
    int status = -1;
    int user_status = -1;
    int sqlite_status = -1;
    uint32_t user_id = 0;
    std::string database_path;
    std::string detail;
    std::vector<TrophySet> sets;
};

bool valid_np_communication_id(const std::string& value);
const char* trophy_grade_name(int grade);
int query_trophy_database(const std::string& database_path,
                          const std::string& np_communication_id,
                          TrophySet& out,
                          std::string& detail);
TrophyLoadResult load_trophies(const std::string& np_communication_id);
TrophyLoadResult load_trophies_for_game(const Game& game, FileSystem& fs);
TrophyCatalogResult load_all_trophy_sets();

}
