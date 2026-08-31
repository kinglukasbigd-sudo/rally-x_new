#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rx {

// Every sound effect in the game is synthesised at run time from square waves
// and noise -- there are no audio files behind them and nothing is sampled
// from the original machine.  The background music is the one exception: it is
// a supplied PCM loop, mixed in underneath at a much lower level.
enum class Sfx {
    Start,
    Flag,          // pitch rises with the flag number
    SpecialFlag,
    LuckyFlag,
    Smoke,
    Crash,
    RoundClear,
    ExtraLife,
    LowFuel,
    ChaseAlarm,    // challenging stage: the tank ran dry, the cars are out
    GameOver
};

// The two looping tracks.  Ordinary rounds and challenging stages each get
// their own, so a bonus stage sounds like the change of pace it is.
enum class MusicTrack { Normal = 0, Challenge = 1, Count = 2 };

class AudioManager {
public:
    // How loud the music sits under the effects.  Effects peak around 0.20 of
    // full scale, so this keeps the music comfortably beneath them.
    static constexpr float MUSIC_GAIN = 0.15f;

    bool init();
    void shutdown();

    // `variant` shifts the pitch; used for the climbing flag blips.
    void play(Sfx s, int variant = 0);

    // Background music: PCM loops read from WAVs, mixed under the effects and
    // repeated seamlessly for as long as they are playing.
    bool loadMusic(MusicTrack track, const std::string& path);

    // Starting the track that is already playing is a no-op, so this can be
    // called every time the state machine moves without restarting the loop.
    void startMusic(MusicTrack track);
    void stopMusic();

    bool musicLoaded(MusicTrack track) const;
    bool musicPlaying() const { return musicPlaying_; }
    MusicTrack currentTrack() const { return current_; }

    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const { return enabled_ && ready_; }

private:
    struct Tone {
        float freq;        // Hz; 0 means noise
        float seconds;
        float volume;      // 0..1
    };

    // One playing sound effect.  Several can overlap, and the music plays
    // underneath all of them.
    struct Voice {
        std::vector<int16_t> data;
        size_t pos    = 0;
        bool   active = false;
    };

    static constexpr int MAX_VOICES = 8;

    static void audioCallback(void* userdata, uint8_t* stream, int len);
    void mix(int16_t* out, int frames);
    void render(const std::vector<Tone>& tones, std::vector<int16_t>& out) const;
    void submit(const std::vector<Tone>& tones);

    uint32_t device_ = 0;
    int      rate_   = 22050;
    bool     ready_  = false;
    bool     enabled_= true;
    mutable uint32_t noiseState_ = 0x13579BDFu;

    std::array<Voice, MAX_VOICES> voices_{};

    std::array<std::vector<int16_t>, static_cast<size_t>(MusicTrack::Count)> tracks_{};
    MusicTrack current_      = MusicTrack::Normal;
    size_t     musicPos_     = 0;
    bool       musicPlaying_ = false;
};

} // namespace rx
