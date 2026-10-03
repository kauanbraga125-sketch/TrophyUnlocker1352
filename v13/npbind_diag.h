#pragma once
#include <string>
#include <vector>

namespace tu {
struct Game;
class FileSystem;
struct NpbindDiagnostic {
    int status = -1;
    std::string path;
    std::vector<std::string> lines;
};
NpbindDiagnostic inspect_npbind_for_game(const Game& game, FileSystem& fs);
}
