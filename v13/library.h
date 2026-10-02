#pragma once
#include "filesystem.h"

namespace tu {
struct Game {
    std::string id, title, path, icon;
    bool package, metadata, manual;
    Game() : package(false), metadata(false), manual(false) {}
};
struct RootResult { std::string path; int status; int matches; };
bool is_cusa(const std::string&);
bool parse_sfo(const std::vector<uint8_t>&, std::string& id, std::string& title);
bool valid_png(const std::vector<uint8_t>&);
std::string game_status(const Game&);
class Library {
    FileSystem& fs;
    void enrich(Game&);
    size_t merge(const Game&);
    std::vector<Game> manuals;
    size_t root_index = 0, enrich_index = 0;
    bool scanning = false;
public:
    std::vector<Game> games;
    std::vector<RootResult> roots;
    explicit Library(FileSystem& f) : fs(f) {}
    void scan();
    void begin_scan();
    bool scan_step();
    bool busy() const { return scanning; }
    int add_id(const std::string&);
    int add_folder(const std::string&);
    int load_manual();
    int save_manual();
    std::string diagnostic(const AccessResult&) const;
};
}
