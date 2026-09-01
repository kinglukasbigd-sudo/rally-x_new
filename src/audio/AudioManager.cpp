#include "audio/AudioManager.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rx {

namespace {
constexpr float MASTER_VOLUME = 0.22f;   // sound-effect level
}

// ---------------------------------------------------------------------------
// Device
// ---------------------------------------------------------------------------

bool AudioManager::init() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "audio unavailable: %s (continuing silently)", SDL_GetError());
        return false;
    }
    SDL_AudioSpec want{};
    want.freq     = rate_;
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = 512;
    want.callback = &AudioManager::audioCallback;
    want.userdata = this;

    SDL_AudioSpec have{};
    // No allowed changes: SDL converts internally, so the mixer below always
    // works in one known format.
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (device_ == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "audio device unavailable: %s (continuing silently)", SDL_GetError());
        return false;
    }
    rate_  = have.freq;
    ready_ = true;
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

void AudioManager::shutdown() {
    if (device_) {
        SDL_PauseAudioDevice(device_, 1);
        SDL_CloseAudioDevice(device_);
    }
    device_ = 0;
    ready_  = false;
}

// ---------------------------------------------------------------------------
// Mixing.  Runs on SDL's audio thread; everything it touches is guarded with
// SDL_LockAudioDevice on the game-thread side.
// ---------------------------------------------------------------------------

void AudioManager::audioCallback(void* userdata, uint8_t* stream, int len) {
    auto* self = static_cast<AudioManager*>(userdata);
    self->mix(reinterpret_cast<int16_t*>(stream), len / static_cast<int>(sizeof(int16_t)));
}

void AudioManager::mix(int16_t* out, int frames) {
    for (int i = 0; i < frames; ++i) {
        int sample = 0;

        const std::vector<int16_t>& music = tracks_[static_cast<size_t>(current_)];
        if (musicPlaying_ && !music.empty() && !musicPaused_) {
            // Muting drops the music out of the mix but keeps the playhead
            // moving, so unmuting rejoins the track instead of restarting it.
            // Pausing freezes the playhead instead, so the game resumes on the
            // same beat it stopped on.
            if (!musicMuted_) sample += static_cast<int>(music[musicPos_] * MUSIC_GAIN);
            if (++musicPos_ >= music.size()) musicPos_ = 0;    // seamless loop
        }

        for (auto& v : voices_) {
            if (!v.active) continue;
            if (!sfxMuted_) sample += v.data[v.pos];
            if (++v.pos >= v.data.size()) { v.active = false; v.pos = 0; }
        }

        out[i] = static_cast<int16_t>(std::clamp(sample, -32768, 32767));
    }
}

// ---------------------------------------------------------------------------
// Sound effects
// ---------------------------------------------------------------------------

void AudioManager::render(const std::vector<Tone>& tones, std::vector<int16_t>& out) const {
    for (const Tone& t : tones) {
        const int samples = static_cast<int>(t.seconds * rate_);
        if (samples <= 0) continue;
        const float period = (t.freq > 0.f) ? (rate_ / t.freq) : 0.f;

        for (int i = 0; i < samples; ++i) {
            // Straight linear decay: hard-edged, like the hardware it evokes.
            const float env = 1.f - static_cast<float>(i) / samples;
            float v;
            if (t.freq > 0.f) {
                v = (std::fmod(static_cast<float>(i), period) < period * 0.5f) ? 1.f : -1.f;
            } else {
                noiseState_ ^= noiseState_ << 13;
                noiseState_ ^= noiseState_ >> 17;
                noiseState_ ^= noiseState_ << 5;
                v = ((noiseState_ >> 16) & 1) ? 1.f : -1.f;
            }
            out.push_back(static_cast<int16_t>(v * env * t.volume * MASTER_VOLUME * 32767.f));
        }
    }
}

void AudioManager::submit(const std::vector<Tone>& tones) {
    if (!enabled() || sfxMuted_) return;

    std::vector<int16_t> buf;
    render(tones, buf);
    if (buf.empty()) return;

    SDL_LockAudioDevice(device_);
    // Take a free voice, or the one that is furthest through if all are busy:
    // a dropped blip is better than a delayed one.
    Voice* slot = nullptr;
    for (auto& v : voices_) if (!v.active) { slot = &v; break; }
    if (!slot) {
        float best = -1.f;
        for (auto& v : voices_) {
            const float progress = v.data.empty() ? 1.f
                                 : static_cast<float>(v.pos) / v.data.size();
            if (progress > best) { best = progress; slot = &v; }
        }
    }
    if (slot) {
        slot->data   = std::move(buf);
        slot->pos    = 0;
        slot->active = true;
    }
    SDL_UnlockAudioDevice(device_);
}

void AudioManager::play(Sfx s, int variant) {
    if (!enabled()) return;

    switch (s) {
        case Sfx::Start:
            submit({ {392.f, 0.09f, 0.9f}, {523.f, 0.09f, 0.9f}, {784.f, 0.16f, 0.9f} });
            break;

        case Sfx::Flag: {
            // Each flag in the sequence sounds a step higher, so the player
            // can hear the run building.
            const float base = 523.f + 42.f * static_cast<float>(variant);
            submit({ {base, 0.045f, 0.8f}, {base * 1.5f, 0.07f, 0.8f} });
            break;
        }

        case Sfx::SpecialFlag:
            submit({ {659.f, 0.06f, 0.9f}, {880.f, 0.06f, 0.9f},
                     {1047.f, 0.06f, 0.9f}, {1319.f, 0.12f, 0.9f} });
            break;

        case Sfx::LuckyFlag:
            submit({ {523.f, 0.05f, 0.9f}, {659.f, 0.05f, 0.9f}, {784.f, 0.05f, 0.9f},
                     {1047.f, 0.05f, 0.9f}, {1319.f, 0.14f, 0.9f} });
            break;

        case Sfx::Smoke:
            submit({ {0.f, 0.07f, 0.45f} });
            break;

        case Sfx::Crash:
            submit({ {0.f, 0.10f, 1.0f}, {196.f, 0.09f, 0.9f},
                     {147.f, 0.10f, 0.8f}, {98.f, 0.22f, 0.7f} });
            break;

        case Sfx::RoundClear:
            submit({ {523.f, 0.10f, 0.9f}, {659.f, 0.10f, 0.9f}, {784.f, 0.10f, 0.9f},
                     {1047.f, 0.24f, 0.9f} });
            break;

        case Sfx::ExtraLife:
            submit({ {784.f, 0.06f, 0.9f}, {1047.f, 0.06f, 0.9f},
                     {784.f, 0.06f, 0.9f}, {1047.f, 0.18f, 0.9f} });
            break;

        case Sfx::LowFuel:
            submit({ {880.f, 0.05f, 0.55f} });
            break;

        case Sfx::ChaseAlarm:
            // A siren climbing twice: unmistakably "get moving".
            submit({ {440.f, 0.10f, 1.0f}, {587.f, 0.10f, 1.0f}, {784.f, 0.12f, 1.0f},
                     {440.f, 0.10f, 1.0f}, {587.f, 0.10f, 1.0f}, {988.f, 0.26f, 1.0f} });
            break;

        case Sfx::GameOver:
            submit({ {392.f, 0.16f, 0.9f}, {330.f, 0.16f, 0.9f},
                     {262.f, 0.16f, 0.9f}, {196.f, 0.34f, 0.9f} });
            break;
    }
}

// ---------------------------------------------------------------------------
// Music
// ---------------------------------------------------------------------------

bool AudioManager::loadMusic(MusicTrack track, const std::string& path) {
    if (!ready_) return false;

    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) return false;                       // no music is not an error

    SDL_AudioSpec spec{};
    uint8_t* data = nullptr;
    uint32_t bytes = 0;
    if (!SDL_LoadWAV_RW(rw, 1, &spec, &data, &bytes)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "music: %s: %s", path.c_str(), SDL_GetError());
        return false;
    }

    // The loop is generated in the device's own format, but convert anyway so
    // a hand-replaced file cannot break playback.
    std::vector<int16_t> pcm;
    if (spec.format == AUDIO_S16SYS && spec.channels == 1 && spec.freq == rate_) {
        pcm.assign(reinterpret_cast<int16_t*>(data),
                   reinterpret_cast<int16_t*>(data) + bytes / sizeof(int16_t));
    } else {
        SDL_AudioCVT cvt;
        if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq,
                              AUDIO_S16SYS, 1, rate_) < 0) {
            SDL_FreeWAV(data);
            return false;
        }
        cvt.len = static_cast<int>(bytes);
        std::vector<uint8_t> buf(static_cast<size_t>(cvt.len) * cvt.len_mult);
        std::copy(data, data + bytes, buf.begin());
        cvt.buf = buf.data();
        if (SDL_ConvertAudio(&cvt) < 0) { SDL_FreeWAV(data); return false; }
        pcm.assign(reinterpret_cast<int16_t*>(buf.data()),
                   reinterpret_cast<int16_t*>(buf.data()) + cvt.len_cvt / sizeof(int16_t));
    }
    SDL_FreeWAV(data);

    const size_t index = static_cast<size_t>(track);
    const size_t frames = pcm.size();

    SDL_LockAudioDevice(device_);
    tracks_[index] = std::move(pcm);
    if (current_ == track) musicPos_ = 0;
    SDL_UnlockAudioDevice(device_);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "music[%zu]: %s (%.1fs, %zu samples)",
                index, path.c_str(), static_cast<double>(frames) / rate_, frames);
    return true;
}

bool AudioManager::musicLoaded(MusicTrack track) const {
    return !tracks_[static_cast<size_t>(track)].empty();
}

void AudioManager::startMusic(MusicTrack track) {
    if (!ready_ || !musicLoaded(track)) return;
    if (musicPlaying_ && current_ == track) return;    // already running

    SDL_LockAudioDevice(device_);
    current_      = track;
    musicPos_     = 0;
    musicPlaying_ = true;
    SDL_UnlockAudioDevice(device_);
}

void AudioManager::setMusicMuted(bool muted) {
    if (musicMuted_ == muted) return;
    if (ready_) SDL_LockAudioDevice(device_);
    musicMuted_ = muted;
    if (ready_) SDL_UnlockAudioDevice(device_);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "music %s", muted ? "muted" : "unmuted");
}

bool AudioManager::toggleMusicMute() {
    setMusicMuted(!musicMuted_);
    return musicMuted_;
}

void AudioManager::setSfxMuted(bool muted) {
    if (sfxMuted_ == muted) return;
    if (ready_) SDL_LockAudioDevice(device_);
    sfxMuted_ = muted;
    if (ready_) SDL_UnlockAudioDevice(device_);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "sound effects %s", muted ? "muted" : "unmuted");
}

bool AudioManager::toggleSfxMute() {
    setSfxMuted(!sfxMuted_);
    return sfxMuted_;
}

void AudioManager::setMusicPaused(bool paused) {
    if (musicPaused_ == paused) return;
    if (ready_) SDL_LockAudioDevice(device_);
    musicPaused_ = paused;
    if (ready_) SDL_UnlockAudioDevice(device_);
}

void AudioManager::stopMusic() {
    if (!ready_ || !musicPlaying_) return;
    SDL_LockAudioDevice(device_);
    musicPlaying_ = false;
    SDL_UnlockAudioDevice(device_);
}

} // namespace rx
