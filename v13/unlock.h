#pragma once
#include <string>

namespace tu {
struct Game;
struct UnlockResult {
    int status = -1;
    int api_status = 0;
    bool restored = false;
    bool already_unlocked = false;
    std::string detail;
};
UnlockResult unlock_trophy_for_game(const Game& game, int trophy_id);
}
