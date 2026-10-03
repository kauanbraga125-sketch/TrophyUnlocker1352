#include "audio.h"
#include <algorithm>
#include <cstring>

namespace tu {

AudioEngine::AudioEngine() = default;
AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::load_clip(const std::string& path, Clip& clip) {
    SDL_AudioSpec got{};
    Uint8* data=nullptr;
    Uint32 len=0;
    if (!SDL_LoadWAV(path.c_str(), &got, &data, &len)) return false;
    if (got.freq != spec_.freq || got.format != spec_.format || got.channels != spec_.channels) {
        SDL_FreeWAV(data);
        return false;
    }
    clip.data=data; clip.len=len; clip.pos=0; clip.active=false;
    return true;
}

void AudioEngine::free_clip(Clip& clip) {
    if (clip.data) SDL_FreeWAV(clip.data);
    clip=Clip{};
}

bool AudioEngine::init(const std::string& base_path) {
    shutdown();
    SDL_AudioSpec want{};
    want.freq=44100;
    want.format=AUDIO_S16LSB;
    want.channels=2;
    want.samples=2048;
    want.callback=&AudioEngine::callback;
    want.userdata=this;
    device_=SDL_OpenAudioDevice(nullptr,0,&want,&spec_,0);
    if (!device_) return false;

    bool ok=true;
    ok &= load_clip(base_path+"/startup.wav",startup_);
    ok &= load_clip(base_path+"/background.wav",background_);
    ok &= load_clip(base_path+"/move.wav",effects_[0]);
    ok &= load_clip(base_path+"/confirm.wav",effects_[1]);
    ok &= load_clip(base_path+"/back.wav",effects_[2]);
    ok &= load_clip(base_path+"/unlock.wav",effects_[3]);
    if (!ok) {
        shutdown();
        return false;
    }
    startup_.active=true;
    startup_done_=false;
    SDL_PauseAudioDevice(device_,0);
    return true;
}

void AudioEngine::play(SoundId id) {
    if (!device_) return;
    int i=static_cast<int>(id);
    if (i < 0 || i >= 4) return;
    SDL_LockAudioDevice(device_);
    effects_[i].pos=0;
    effects_[i].active=true;
    SDL_UnlockAudioDevice(device_);
}

void AudioEngine::callback(void* userdata, Uint8* stream, int len) {
    static_cast<AudioEngine*>(userdata)->mix(stream,len);
}

void AudioEngine::mix(Uint8* stream, int len) {
    std::memset(stream,0,size_t(len));
    if (!device_) return;

    auto mix_clip=[&](Clip& clip,int volume,bool loop) {
        int remaining=len;
        int outoff=0;
        while (remaining > 0 && clip.data && clip.len) {
            if (clip.pos >= clip.len) {
                if (loop) clip.pos=0;
                else { clip.active=false; break; }
            }
            Uint32 available=clip.len-clip.pos;
            int take=std::min<int>(remaining,int(available));
            SDL_MixAudioFormat(stream+outoff,clip.data+clip.pos,spec_.format,Uint32(take),volume);
            clip.pos += Uint32(take);
            outoff += take;
            remaining -= take;
        }
    };

    if (!startup_done_) {
        if (startup_.active) mix_clip(startup_,SDL_MIX_MAXVOLUME, false);
        if (!startup_.active) {
            startup_done_=true;
            background_.pos=0;
            background_.active=true;
        }
    }
    if (startup_done_) mix_clip(background_,92,true);

    for (Clip& fx : effects_) if (fx.active) mix_clip(fx,SDL_MIX_MAXVOLUME,false);
}

void AudioEngine::shutdown() {
    if (device_) {
        SDL_PauseAudioDevice(device_,1);
        SDL_LockAudioDevice(device_);
    }
    free_clip(startup_);
    free_clip(background_);
    for (Clip& fx : effects_) free_clip(fx);
    if (device_) {
        SDL_UnlockAudioDevice(device_);
        SDL_CloseAudioDevice(device_);
        device_=0;
    }
    startup_done_=false;
}

} // namespace tu
