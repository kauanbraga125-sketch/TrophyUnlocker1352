#pragma once
#include <SDL2/SDL.h>
#include <string>

namespace tu {

enum class SoundId { Move=0, Confirm=1, Back=2, Unlock=3 };

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    bool init(const std::string& base_path);
    void play(SoundId id);
    void shutdown();

private:
    struct Clip {
        Uint8* data=nullptr;
        Uint32 len=0;
        Uint32 pos=0;
        bool active=false;
    };

    SDL_AudioDeviceID device_=0;
    SDL_AudioSpec spec_{};
    Clip startup_;
    Clip background_;
    Clip effects_[4];
    bool startup_done_=false;

    static void callback(void* userdata, Uint8* stream, int len);
    void mix(Uint8* stream, int len);
    bool load_clip(const std::string& path, Clip& clip);
    void free_clip(Clip& clip);
};

} // namespace tu
