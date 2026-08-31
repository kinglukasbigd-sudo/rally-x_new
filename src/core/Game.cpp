#include "core/Game.h"
#include "rendering/Palette.h"
#include "world/LevelLoader.h"
#include "core/FileSystem.h"
#include <SDL.h>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <sys/stat.h>

namespace rx {

namespace {
constexpr float READY_SECONDS          = 2.0f;
constexpr float ROUND_COMPLETE_SECONDS = 2.5f;
constexpr float DEATH_SECONDS          = 2.0f;
// Title-screen control-scheme toggle, in framebuffer pixels.
constexpr int   TOGGLE_TEXT_Y          = 178;
constexpr int   TOGGLE_BOX_X           = 82;
constexpr int   TOGGLE_BOX_W           = 124;
constexpr int   TOGGLE_BOX_Y           = TOGGLE_TEXT_Y - 5;
constexpr int   TOGGLE_BOX_H           = 18;
constexpr float GAME_OVER_SECONDS      = 3.0f;

bool fileExists(const std::string& p) { return FileSystem::exists(p); }

// Create the folder if it is not there yet.  This matters on Android: a
// directory made from outside the app (adb, a script) is owned by whoever made
// it and the app is then denied access to its own folder.  Made by the app, it
// belongs to the app, and anything dropped in afterwards can be read.
void ensureDir(const std::string& path) {
    std::string p = path;
    while (!p.empty() && p.back() == '/') p.pop_back();
    if (p.empty()) return;
    ::mkdir(p.c_str(), 0777);
}

// Where the player can drop their own music, checked before the bundled
// tracks.  Anything here belongs to whoever put it there: it is never part of
// the project, never shipped in the APK, and never committed.
std::vector<std::string> userMusicDirs() {
    std::vector<std::string> dirs;

#if defined(__ANDROID__)
    // The app's own external folder, reachable over USB or a file manager at
    //   Android/data/com.cleanroom.newrallyx/files/music/
    if (const char* ext = SDL_AndroidGetExternalStoragePath())
        dirs.push_back(std::string(ext) + "/music/");
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"))
        dirs.push_back(std::string(xdg) + "/newrallyx/music/");
    if (const char* home = std::getenv("HOME"))
        dirs.push_back(std::string(home) + "/.local/share/newrallyx/music/");
#endif
    dirs.push_back("music/");          // or just a folder next to the game
    return dirs;
}

// The one the player is told about, and the one the game creates for them.
std::string primaryMusicDir() {
    const auto dirs = userMusicDirs();
    return dirs.empty() ? std::string("music/") : dirs.front();
}

// The looping background tracks.  A user-supplied file always wins; otherwise
// the bundled loop is used.  Inside an APK the assets live at the root, so the
// bare "audio/..." path is the one that resolves there.
std::string findAudio(const std::string& name) {
    for (const std::string& dir : userMusicDirs()) {
        const std::string path = dir + name;
        const bool found = fileExists(path);
        // Logged either way: when someone's own track is not picked up, the
        // first question is always which paths were actually looked at.
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "music search: %s%s",
                    path.c_str(), found ? "  <= using this" : "  (not found)");
        if (found) return path;
    }

    const std::string candidates[] = {
        "audio/" + name,
        "assets/audio/" + name,
        "../assets/audio/" + name,
        "../../assets/audio/" + name,
    };
    for (const std::string& c : candidates)
        if (fileExists(c)) return c;
    return "";
}

// The game is normally launched from the project root, but tolerate being run
// from build/ or an installed location.
std::string findDataDir(const std::string& preferred) {
    const char* env = std::getenv("RALLYX_DATA");
    if (env && fileExists(std::string(env) + "/level01.lvl")) return env;
    // "levels" is the one that resolves inside an APK, so it comes first
    // after any explicit override.
    const std::string candidates[] = { preferred, "levels", "../levels", "../../levels" };
    for (const auto& c : candidates)
        if (fileExists(c + "/level01.lvl")) return c;
    return preferred;
}
} // namespace

bool Game::init(int scale, const std::string& dataDir, int startRound,
                bool fullscreen, bool touchUi, TouchScheme scheme, uint32_t seed) {
    dataDir_ = findDataDir(dataDir);
    startRound_ = std::max(1, startRound);
    gameSeed_ = seed ? seed : static_cast<uint32_t>(SDL_GetPerformanceCounter());
    levelCount_ = 0;
    for (int i = 1; i <= 99; ++i) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%s/level%02d.lvl", dataDir_.c_str(), i);
        if (!fileExists(buf)) break;
        ++levelCount_;
    }
    if (levelCount_ == 0)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "no level data under '%s', using fallback maze", dataDir_.c_str());

    if (!renderer_.init("New Rally-X", scale, fullscreen)) return false;

    // On a touch device there is no keyboard, so the on-screen pad is the only
    // way to drive.  It can also be forced on for testing with --touch.
    hasTouchDevice_ = SDL_GetNumTouchDevices() > 0;
    touch_.setEnabled(touchUi || hasTouchDevice_);
    touch_.setScheme(scheme);

    // Fullscreen fills the display; a window keeps whole-number pixels.
    renderer_.setFillScreen(fullscreen);
    sprites_.create(renderer_);
    audio_.init();          // silence is an acceptable outcome, not an error
    // Make the folder before looking in it, and say where it is: "drop a wav
    // here" is useless advice without the path.
    ensureDir(primaryMusicDir());
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "your own music goes in: %s  (music_normal.wav, music_challenge.wav)",
                primaryMusicDir().c_str());

    struct TrackFile { MusicTrack track; const char* file; };
    for (const TrackFile& t : { TrackFile{ MusicTrack::Normal,    "music_normal.wav" },
                                TrackFile{ MusicTrack::Challenge, "music_challenge.wav" } }) {
        const std::string path = findAudio(t.file);
        if (path.empty())
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "no %s found; that state plays without music", t.file);
        else
            audio_.loadMusic(t.track, path);
    }

    startNewGame();
    setState(GameState::StartScreen);
    running_ = true;
    return true;
}

void Game::shutdown() {
    audio_.shutdown();
    sprites_.destroy(renderer_);
    renderer_.shutdown();
}

std::string Game::levelPath(int roundNumber) const {
    if (levelCount_ <= 0) return "";
    // Rounds beyond the authored set cycle back round, as arcade games do.
    const int idx = ((roundNumber - 1) % levelCount_) + 1;
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s/level%02d.lvl", dataDir_.c_str(), idx);
    return buf;
}

uint32_t Game::flagSeedFor(int roundNumber) const {
    // Mix the game seed with the round so every round of every game differs,
    // while a given --seed still replays exactly.
    uint32_t s = gameSeed_ ^ (static_cast<uint32_t>(roundNumber) * 2654435761u);
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s ? s : 1u;
}

void Game::startNewGame() {
    score_.newGame();
    lives_.reset(START_LIVES);
    roundNumber_ = startRound_;
    deathCause_ = DeathCause::None;
    challengeBonusShown_ = 0;
    challengePerfect_ = false;
    loadRound(roundNumber_);
}

void Game::onPlayerDied(DeathCause cause) {
    deathCause_ = cause;
    score_.onPlayerDeath();
    audio_.play(Sfx::Crash);
    setState(GameState::PlayerDeath);
}

void Game::playRoundSounds(const Round::Events& ev) {
    if (ev.flagsTaken > 0)  audio_.play(Sfx::Flag, ev.flagSequence - 1);
    if (ev.specialTaken)    audio_.play(Sfx::SpecialFlag);
    if (ev.luckyTaken)      audio_.play(Sfx::LuckyFlag);
    if (ev.smokePuffed)     audio_.play(Sfx::Smoke);
}

void Game::updateLowFuelWarning(float dt) {
    // The fuel alarm: a steady beep once the tank drops into the last quarter.
    if (round_.fuel().fraction() >= 0.25f || round_.fuel().infinite()) {
        lowFuelTimer_ = 0.f;
        return;
    }
    lowFuelTimer_ -= dt;
    if (lowFuelTimer_ <= 0.f) {
        audio_.play(Sfx::LowFuel);
        lowFuelTimer_ = 0.55f;
    }
}

void Game::loadRound(int roundNumber) {
    Maze maze;
    const std::string path = levelPath(roundNumber);
    if (path.empty() || !maze.load(path)) maze.loadFallback();

    LevelData data = maze.data();

    // Each full cycle through the authored levels raises the difficulty a step,
    // which is how the original keeps going past its last hand-made layout.
    const int lap = (levelCount_ > 0) ? (roundNumber - 1) / levelCount_ : 0;
    data.difficulty += lap;
    data.enemySpeed += 0.06f * lap;
    // The tank stays at whatever the level data says (always 100); every lap
    // past the authored set makes it burn faster instead of making it smaller,
    // so the gauge always starts full and only the clock behind it speeds up.
    data.fuelDrain  *= (1.f + 0.15f * lap);
    data.fuelDrain   = std::min(data.fuelDrain, 4.0f);   // never under ~25s a round
    // The pursuit must never out-run the player, or a round becomes unwinnable.
    data.enemySpeed  = std::min(data.enemySpeed, data.playerSpeed - 0.05f);

    round_.load(data, flagSeedFor(roundNumber));
    score_.newRound();
    theme_ = themeFor(roundNumber);
}

void Game::setState(GameState s) {
    state_ = s;
    stateTimer_ = 0.f;

    // Each kind of round has its own track; everything in between -- the title
    // screen, the READY card, a death, a cleared round -- stays quiet so the
    // sound effects there are not competing with anything.
    if      (s == GameState::Playing)          audio_.startMusic(MusicTrack::Normal);
    else if (s == GameState::ChallengingStage) audio_.startMusic(MusicTrack::Challenge);
    else                                       audio_.stopMusic();
}

void Game::run() {
    uint64_t prev = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    double accumulator = 0.0;

    while (running_) {
        const uint64_t now = SDL_GetPerformanceCounter();
        double frame = static_cast<double>(now - prev) / freq;
        prev = now;
        if (frame > 0.25) frame = 0.25;            // never spiral after a stall
        if (frame > 0.0) fps_ = fps_ * 0.9f + static_cast<float>(0.1 / frame);

        accumulator += frame;

        handleEvents();

        // Fixed simulation step: gameplay never sees a variable frame time.
        while (accumulator >= FIXED_DT) {
            fixedUpdate(static_cast<float>(FIXED_DT));
            accumulator -= FIXED_DT;
        }

        render();
    }
}

void Game::runCapture(const std::string& outDir, int frames, int everyN) {
    // A crude scripted driver: hold a direction for a while, then pick another.
    static const Action kScript[] = { Action::Right, Action::Down, Action::Left,
                                      Action::Up,    Action::Right, Action::Down };
    int shot = 0;
    for (int f = 0; f < frames; ++f) {
        input_.beginFrame();

        if (state_ == GameState::StartScreen && f > 150)
            input_.setFromExternal(Action::Start, true);
        else
            input_.setFromExternal(Action::Start, false);

        const Action want = kScript[(f / 90) % (sizeof kScript / sizeof kScript[0])];
        for (Action a : { Action::Up, Action::Down, Action::Left, Action::Right })
            input_.setFromExternal(a, a == want);
        input_.setFromExternal(Action::Smoke, (f % 150) == 0 && f > 200);

        fixedUpdate(static_cast<float>(FIXED_DT));
        render();

        if (everyN > 0 && f % everyN == 0) {
            char buf[256];
            std::snprintf(buf, sizeof buf, "%s/frame%03d.bmp", outDir.c_str(), shot++);
            renderer_.saveScreenshot(buf);
        }
    }
    std::printf("capture: %d frames, %d shots, state=%s score=%d flags=%d/%d\n",
                frames, shot, stateName(state_), score_.score(),
                round_.flagsCollected(), round_.flagsRequired());
}

void Game::handleEvents() {
    input_.beginFrame();

    int winW = 0, winH = 0;
    renderer_.windowSize(winW, winH);

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) running_ = false;

        // Android's back button arrives as a key, and it means "leave".
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_AC_BACK) running_ = false;

        if (touch_.enabled()) {
            // Finger coordinates are normalised to the window; the pad works
            // in window pixels so it can sit beside the playfield.
            switch (e.type) {
                case SDL_FINGERDOWN:
                case SDL_FINGERMOTION: {
                    const float x = e.tfinger.x * winW, y = e.tfinger.y * winH;
                    if (e.type == SDL_FINGERDOWN) {
                        if (state_ == GameState::StartScreen &&
                            schemeToggleRect_.w > 0.f &&
                            x >= schemeToggleRect_.x && x < schemeToggleRect_.x + schemeToggleRect_.w &&
                            y >= schemeToggleRect_.y && y < schemeToggleRect_.y + schemeToggleRect_.h) {
                            touch_.setScheme(touch_.scheme() == TouchScheme::Swipe
                                             ? TouchScheme::Pad : TouchScheme::Swipe);
                            audio_.play(Sfx::Flag);
                            break;                       // not a start tap
                        }
                        touch_.fingerDown(e.tfinger.fingerId, x, y);
                    } else {
                        touch_.fingerMove(e.tfinger.fingerId, x, y);
                    }
                    break;
                }
                case SDL_FINGERUP:
                    touch_.fingerUp(e.tfinger.fingerId);
                    break;

                // Mouse stands in for a finger so the schemes can be tried on a
                // desktop.  Ignored outright where a real touchscreen exists.
                case SDL_MOUSEBUTTONDOWN: {
                    if (hasTouchDevice_) break;
                    const float x = static_cast<float>(e.button.x), y = static_cast<float>(e.button.y);
                    if (state_ == GameState::StartScreen && schemeToggleRect_.w > 0.f &&
                        x >= schemeToggleRect_.x && x < schemeToggleRect_.x + schemeToggleRect_.w &&
                        y >= schemeToggleRect_.y && y < schemeToggleRect_.y + schemeToggleRect_.h) {
                        touch_.setScheme(touch_.scheme() == TouchScheme::Swipe
                                         ? TouchScheme::Pad : TouchScheme::Swipe);
                        audio_.play(Sfx::Flag);
                        break;
                    }
                    touch_.fingerDown(-1, x, y);
                    break;
                }
                case SDL_MOUSEMOTION:
                    if (hasTouchDevice_) break;
                    if (e.motion.state & SDL_BUTTON_LMASK)
                        touch_.fingerMove(-1, static_cast<float>(e.motion.x), static_cast<float>(e.motion.y));
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (hasTouchDevice_) break;
                    touch_.fingerUp(-1);
                    break;

                case SDL_APP_WILLENTERBACKGROUND:
                    touch_.releaseAll();      // never leave a direction stuck on
                    break;
                default: break;
            }
        }

        input_.handleEvent(e);
    }

    if (touch_.enabled()) {
        touch_.applyTo(input_);

        // A tap means different things depending on where the game is.  While
        // a round is running it is the smoke button -- in swipe mode there is
        // no button to press, so tapping the screen has to be it.  Anywhere
        // else it is Start.
        const bool tap = touch_.consumeTap();
        const bool inPlay = (state_ == GameState::Playing ||
                             state_ == GameState::ChallengingStage);

        input_.setFromExternal(Action::Start, tap && !inPlay);
        if (tap && inPlay) input_.setFromExternal(Action::Smoke, true);
    }
    if (input_.pressed(Action::Quit)) running_ = false;
    if (input_.pressed(Action::Debug)) debug_.enabled = !debug_.enabled;

    if (input_.debugKeyPressed(1)) debug_.collisionBoxes = !debug_.collisionBoxes;
    if (input_.debugKeyPressed(2)) debug_.navGraph       = !debug_.navGraph;
    if (input_.debugKeyPressed(3)) debug_.enemyTargets   = !debug_.enemyTargets;
    if (input_.debugKeyPressed(4)) debug_.coordinates    = !debug_.coordinates;
    if (input_.debugKeyPressed(5)) debug_.radarDebug     = !debug_.radarDebug;
    if (input_.debugKeyPressed(6)) debug_.infiniteFuel   = !debug_.infiniteFuel;
    if (input_.debugKeyPressed(7)) debug_.freezeEnemies  = !debug_.freezeEnemies;
    if (input_.debugKeyPressed(8)) debugCollectAllFlags();
    if (input_.debugKeyPressed(9)) { loadRound(roundNumber_); setState(GameState::Ready); }
}

void Game::step(float dt) { fixedUpdate(dt); }

void Game::fixedUpdate(float dt) {
    ++tick_;
    stateTimer_ += dt;
    touch_.update(dt);

    switch (state_) {
        case GameState::StartScreen:
            if (input_.pressed(Action::Start) || input_.pressed(Action::Smoke)) {
                startNewGame();
                audio_.play(Sfx::Start);
                setState(GameState::Ready);
            }
            break;

        case GameState::Ready:
            luckyBonusShown_ = 0;
            if (stateTimer_ >= READY_SECONDS) {
                if (round_.isChallenge()) {
                    challenge_.begin();
                    setState(GameState::ChallengingStage);
                } else {
                    setState(GameState::Playing);
                }
            }
            break;

        case GameState::Playing: {
            round_.setInfiniteFuel(debug_.infiniteFuel);
            round_.setEnemiesFrozen(debug_.freezeEnemies);
            const auto ev = round_.update(input_, score_, dt);
            if (ev.bonusAwarded > 0) luckyBonusShown_ = ev.bonusAwarded;
            playRoundSounds(ev);
            updateLowFuelWarning(dt);
            if (lives_.checkBonusLife(score_.score())) audio_.play(Sfx::ExtraLife);

            if (ev.playerDied)         onPlayerDied(ev.cause);
            else if (ev.roundComplete) { audio_.play(Sfx::RoundClear);
                                         setState(GameState::RoundComplete); }
            break;
        }

        case GameState::ChallengingStage: {
            round_.setInfiniteFuel(debug_.infiniteFuel);
            const auto ev = round_.update(input_, score_, dt);
            if (ev.bonusAwarded > 0) luckyBonusShown_ = ev.bonusAwarded;
            playRoundSounds(ev);
            updateLowFuelWarning(dt);
            if (lives_.checkBonusLife(score_.score())) audio_.play(Sfx::ExtraLife);
            if (ev.chaseStarted) audio_.play(Sfx::ChaseAlarm);

            const auto out = challenge_.update(round_, ev.playerDied, ev.roundComplete, score_);
            if (out.finished) {
                challengeBonusShown_ = out.bonus;
                challengePerfect_    = out.perfect;
                audio_.play(out.perfect ? Sfx::RoundClear : Sfx::Crash);
                setState(GameState::RoundComplete);   // never costs a car
            }
            break;
        }

        case GameState::PlayerDeath:
            if (stateTimer_ >= DEATH_SECONDS) {
                if (lives_.loseLife()) {
                    // Keep the flags already banked, and keep the scoring
                    // ladder where it was: a lost car costs a car, not the
                    // round's progress.
                    round_.restartAfterDeath();
                    setState(GameState::Ready);
                } else {
                    audio_.play(Sfx::GameOver);
                    setState(GameState::GameOver);
                }
            }
            break;

        case GameState::RoundComplete:
            if (stateTimer_ >= ROUND_COMPLETE_SECONDS) {
                challengeBonusShown_ = 0;
                challengePerfect_ = false;
                ++roundNumber_;
                loadRound(roundNumber_);
                setState(GameState::Ready);
            }
            break;

        case GameState::GameOver:
            if (stateTimer_ >= GAME_OVER_SECONDS ||
                (stateTimer_ > 1.f && input_.pressed(Action::Start)))
                setState(GameState::StartScreen);
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void Game::render() {
    renderer_.beginFrame();
    renderer_.clear(pal::Black);

    if (state_ == GameState::StartScreen) {
        renderStartScreen();
    } else {
        renderWorld();

        HudInfo info;
        info.score          = score_.score();
        info.highScore      = score_.highScore();
        info.lives          = lives_.lives();
        info.round          = roundNumber_;
        info.fuel           = round_.fuel().fuel();
        info.fuelMax        = round_.fuel().capacity();
        info.flagsRemaining = round_.flagsRemaining();
        info.multiplier     = score_.multiplier();
        info.challenge      = round_.isChallenge();
        hud_.draw(renderer_, info);
        radar_.draw(renderer_, round_, tick_, debug_.radarDebug, theme_);

        renderOverlayText();
    }

    if (debug_.enabled) renderDebug();

    renderer_.beginPresent();
    if (touch_.enabled()) {
        drawTouchControls();

        // The controls toggle lives on the title screen only.  Its hit box is
        // the on-screen position of that text row, mapped out of framebuffer
        // space into window pixels.
        if (state_ == GameState::StartScreen) {
            const Rect g = renderer_.gameRect();
            const float sx = g.w / SCREEN_W, sy = g.h / SCREEN_H;
            schemeToggleRect_ = Rect{ g.x + TOGGLE_BOX_X * sx, g.y + TOGGLE_BOX_Y * sy,
                                      TOGGLE_BOX_W * sx, TOGGLE_BOX_H * sy };
        } else {
            schemeToggleRect_ = Rect{};
        }
    }
    renderer_.endPresent();
}

void Game::drawTouchControls() {
    // Drawn in window pixels, after the playfield has been blitted, so the
    // pad can use the space either side of a 288x224 picture on a phone.
    int winW = 0, winH = 0;
    renderer_.windowSize(winW, winH);
    touch_.layout(winW, winH, renderer_.gameRect());

    if (touch_.scheme() == TouchScheme::Swipe) { drawSwipeFeedback(); return; }

    // A filled triangle, built from rows: no anti-aliasing, no curves.
    auto arrow = [&](const Rect& r, Direction dir, Color c) {
        const int steps = static_cast<int>(r.h * 0.34f);
        for (int i = 0; i < steps; ++i) {
            const int span = (i + 1) * 2;
            const int cx = static_cast<int>(r.x + r.w * 0.5f);
            const int cy = static_cast<int>(r.y + r.h * 0.5f);
            switch (dir) {
                case Direction::Up:    renderer_.fillRect(cx - i, cy - steps / 2 + i, span, 1, c); break;
                case Direction::Down:  renderer_.fillRect(cx - i, cy + steps / 2 - i, span, 1, c); break;
                case Direction::Left:  renderer_.fillRect(cx - steps / 2 + i, cy - i, 1, span, c); break;
                case Direction::Right: renderer_.fillRect(cx + steps / 2 - i, cy - i, 1, span, c); break;
                default: break;
            }
        }
    };

    struct Btn { Action action; Direction dir; };
    const Btn pad[4] = { { Action::Up,    Direction::Up    },
                         { Action::Down,  Direction::Down  },
                         { Action::Left,  Direction::Left  },
                         { Action::Right, Direction::Right } };

    for (const Btn& b : pad) {
        const Rect& r = touch_.rect(b.action);
        if (r.w <= 0.f) continue;
        const bool on = touch_.pressed(b.action);
        const Color fill    = on ? Color{ 60, 150, 255, 110 } : Color{ 255, 255, 255, 26 };
        const Color outline = on ? Color{ 160, 210, 255, 220 } : Color{ 200, 200, 210, 90 };
        renderer_.fillRect(static_cast<int>(r.x), static_cast<int>(r.y),
                           static_cast<int>(r.w), static_cast<int>(r.h), fill);
        renderer_.drawRect(static_cast<int>(r.x), static_cast<int>(r.y),
                           static_cast<int>(r.w), static_cast<int>(r.h), outline);
        arrow(r, b.dir, on ? Color{ 255, 255, 255, 240 } : Color{ 220, 220, 230, 130 });
    }

    const Rect& s = touch_.rect(Action::Smoke);
    if (s.w > 0.f) {
        const bool on = touch_.pressed(Action::Smoke);
        renderer_.fillRect(static_cast<int>(s.x), static_cast<int>(s.y),
                           static_cast<int>(s.w), static_cast<int>(s.h),
                           on ? Color{ 224, 224, 224, 120 } : Color{ 255, 255, 255, 26 });
        renderer_.drawRect(static_cast<int>(s.x), static_cast<int>(s.y),
                           static_cast<int>(s.w), static_cast<int>(s.h),
                           on ? Color{ 255, 255, 255, 230 } : Color{ 200, 200, 210, 90 });
        // Three puffs: the smoke button says what it does without a caption.
        const int cx = static_cast<int>(s.x + s.w * 0.5f);
        const int cy = static_cast<int>(s.y + s.h * 0.5f);
        const int d  = static_cast<int>(s.w * 0.16f);
        const Color puff = on ? Color{ 255, 255, 255, 240 } : Color{ 220, 220, 230, 140 };
        renderer_.fillRect(cx - d * 2, cy - d / 2, d, d, puff);
        renderer_.fillRect(cx - d / 2, cy - d,     d + d / 2, d + d / 2, puff);
        renderer_.fillRect(cx + d,     cy - d / 2, d, d, puff);
    }
}

// The only thing the swipe scheme puts on screen: a square that fills up while
// a finger is held, so the two-second wait for smoke is visible rather than
// guessed at.  Nothing is drawn over the playfield during normal play.
void Game::drawSwipeFeedback() {
    if (!touch_.holdCharging() && !touch_.smoking()) return;

    int winW = 0, winH = 0;
    renderer_.windowSize(winW, winH);
    const float unit = std::min(winW, winH) * 0.075f;
    const Vec2  p    = touch_.holdPoint();

    const int x = static_cast<int>(p.x - unit * 0.5f);
    const int y = static_cast<int>(p.y - unit * 0.5f);
    const int w = static_cast<int>(unit);

    if (touch_.smoking()) {
        renderer_.fillRect(x, y, w, w, Color{ 224, 224, 224, 120 });
        renderer_.drawRect(x, y, w, w, Color{ 255, 255, 255, 220 });
        return;
    }

    // Charging: a hard-edged bar climbing the square, no easing, no glow.
    const int filled = static_cast<int>(w * touch_.holdProgress());
    renderer_.drawRect(x, y, w, w, Color{ 200, 200, 210, 120 });
    if (filled > 0)
        renderer_.fillRect(x, y + (w - filled), w, filled, Color{ 224, 224, 224, 90 });
}

void Game::renderStartScreen() {
    const int cx = SCREEN_W / 2;

    renderer_.textCentered(cx, 44,  "NEW RALLY-X", pal::Accent);
    renderer_.fillRect(cx - 46, 56, 92, 1, pal::Accent);
    renderer_.textCentered(cx, 66,  "1981", pal::TextDim);

    // A blue car being chased across the screen, the way an attract mode
    // would show it.  Pure decoration: it drives on a straight line.
    const int lane = 112;
    const int span = SCREEN_W + 64;
    const int lead = ((tick_ * 2) % span) - 32;
    const int frame = (tick_ / 5) & 1;
    renderer_.fillRect(0, lane + 12, SCREEN_W, 1, pal::WallDark);
    sprites_.drawCarAtScreen(renderer_, lead, lane, Direction::Right, frame, false);
    sprites_.drawCarAtScreen(renderer_, lead - 34, lane, Direction::Right, frame, true, 0);
    sprites_.drawCarAtScreen(renderer_, lead - 58, lane, Direction::Right, frame ^ 1, true, 4);

    if ((tick_ / 30) % 2 == 0)
        renderer_.textCentered(cx, 146, "PRESS START", pal::Text);

    if (touch_.enabled()) {
        const bool swipe = (touch_.scheme() == TouchScheme::Swipe);
        renderer_.textCentered(cx, 158, "TAP TO START", pal::TextDim);

        // The scheme toggle sits mid-screen on purpose: the bottom edge of a
        // phone belongs to the system's navigation gestures, and a control
        // parked there simply never gets the touch.
        renderer_.textCentered(cx, TOGGLE_TEXT_Y, swipe ? "[ CONTROLS: SWIPE ]"
                                                        : "[ CONTROLS: PAD ]", pal::Accent);

        renderer_.textCentered(cx, 200, swipe ? "SWIPE TO DRIVE" : "PAD TO DRIVE", pal::TextDim);
        renderer_.textCentered(cx, 210, swipe ? "TAP FOR SMOKE"
                                              : "BUTTON FOR SMOKE", pal::TextDim);
    } else {
        renderer_.textCentered(cx, 164, "ENTER OR SPACE", pal::TextDim);
        renderer_.textCentered(cx, 188, "ARROWS DRIVE   SPACE SMOKE", pal::TextDim);
        renderer_.textCentered(cx, 202, "COLLECT 10 FLAGS PER ROUND", pal::TextDim);
        renderer_.textCentered(cx, 214, "CLEAN-ROOM RECONSTRUCTION", pal::TextDim);
    }

    if (score_.highScore() > 0) {
        renderer_.textCentered(cx, 4, "HIGH SCORE", pal::Accent);
        renderer_.textCentered(cx, 14, padNumber(score_.highScore(), 6), pal::Text);
    }
}

void Game::renderWorld() {
    renderer_.setClip(VIEW_X, VIEW_Y, VIEW_W, VIEW_H);
    renderer_.fillRect(VIEW_X, VIEW_Y, VIEW_W, VIEW_H, pal::Road);

    const Camera& cam = round_.camera();
    sprites_.drawMaze(renderer_, round_.map(), cam, theme_);

    for (const auto& rk : round_.rocks())
        if (cam.visible(rk.pos.x, rk.pos.y)) sprites_.drawRock(renderer_, cam, rk.pos);

    for (const auto& f : round_.flags())
        if (!f.collected && cam.visible(f.pos.x, f.pos.y))
            sprites_.drawFlag(renderer_, cam, f);

    for (const auto& c : round_.smoke().clouds())
        if (c.active && cam.visible(c.pos.x, c.pos.y))
            sprites_.drawSmoke(renderer_, cam, c.pos, c.lifeFraction());

    for (const auto& e : round_.enemies()) {
        // Cars waiting to launch are drawn too: the player should see the pack
        // sitting in the pen, and in a challenging stage should see them
        // arrive rather than have them appear already moving.
        if (!e.onTrack()) continue;
        if (!cam.visible(e.position().x, e.position().y)) continue;
        sprites_.drawEnemy(renderer_, cam, e.position(), e.direction(),
                           e.animationFrame(), e.stunned(),
                           static_cast<int>(e.stunTimer() * 60.f), theme_.index);
    }

    if (debug_.enemyTargets) {
        for (const auto& e : round_.enemies()) {
            if (!e.dangerous()) continue;
            renderer_.drawLine(VIEW_X + static_cast<int>(cam.toScreenX(e.position().x)),
                               VIEW_Y + static_cast<int>(cam.toScreenY(e.position().y)),
                               VIEW_X + static_cast<int>(cam.toScreenX(e.target().x)),
                               VIEW_Y + static_cast<int>(cam.toScreenY(e.target().y)),
                               pal::Danger);
        }
    }

    if (debug_.navGraph) {
        for (const auto& n : round_.nav().nodes()) {
            const float wx = n.tx * TILE + TILE * 0.5f, wy = n.ty * TILE + TILE * 0.5f;
            if (!cam.visible(wx, wy)) continue;
            renderer_.fillRect(VIEW_X + static_cast<int>(cam.toScreenX(wx)) - 1,
                               VIEW_Y + static_cast<int>(cam.toScreenY(wy)) - 1,
                               3, 3, pal::FlagLucky);
        }
    }

    // On death the car spins on the spot in 90-degree steps, which keeps the
    // pixel grid exact while still reading as a wreck.
    if (state_ == GameState::PlayerDeath) {
        static const Direction kSpin[4] = { Direction::Up, Direction::Right,
                                            Direction::Down, Direction::Left };
        const int step = static_cast<int>(stateTimer_ * 10.f) % 4;
        if (stateTimer_ < 1.4f)
            sprites_.drawPlayer(renderer_, cam, round_.player().position(),
                                kSpin[step], round_.player().animationFrame());
    } else {
        sprites_.drawPlayer(renderer_, cam, round_.player().position(),
                            round_.player().direction(), round_.player().animationFrame());
    }

    if (debug_.coordinates) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%d,%d",
                      static_cast<int>(round_.player().position().x),
                      static_cast<int>(round_.player().position().y));
        renderer_.text(VIEW_X + static_cast<int>(cam.toScreenX(round_.player().position().x)) + 10,
                       VIEW_Y + static_cast<int>(cam.toScreenY(round_.player().position().y)) - 4,
                       buf, pal::FlagSpecial);
    }

    if (debug_.collisionBoxes) {
        auto box = [&](const Rect& b, Color c) {
            renderer_.drawRect(VIEW_X + static_cast<int>(cam.toScreenX(b.x)),
                               VIEW_Y + static_cast<int>(cam.toScreenY(b.y)),
                               static_cast<int>(b.w), static_cast<int>(b.h), c);
        };
        box(round_.player().bounds(), pal::FlagSpecial);
        for (const auto& e : round_.enemies()) if (e.onTrack()) box(e.bounds(), pal::Danger);
        for (const auto& rk : round_.rocks()) box(rk.bounds(), pal::Rock);
        for (const auto& f : round_.flags()) if (!f.collected) box(f.bounds(), pal::FlagNormal);
    }

    renderer_.clearClip();
}

void Game::renderOverlayText() {
    const int cx = VIEW_X + VIEW_W / 2;

    // A plain black bar behind the message so it stays readable over the maze.
    auto banner = [&](int top, int height) {
        renderer_.fillRect(VIEW_X, VIEW_Y + VIEW_H / 2 + top, VIEW_W, height, pal::Black);
    };

    switch (state_) {
        case GameState::Ready:
            banner(-18, 44);
            renderer_.textCentered(cx, VIEW_Y + VIEW_H / 2 - 14,
                                   round_.isChallenge() ? "CHALLENGING STAGE"
                                                        : ("ROUND " + std::to_string(roundNumber_)),
                                   pal::Accent);
            if ((tick_ / 15) % 2 == 0)
                renderer_.textCentered(cx, VIEW_Y + VIEW_H / 2 + 2, "READY", pal::Text);
            if (round_.isChallenge())
                renderer_.textCentered(cx, VIEW_Y + VIEW_H / 2 + 18, "NO ENEMY CARS", pal::TextDim);
            else
                renderer_.textCentered(cx, VIEW_Y + VIEW_H / 2 + 18,
                                       std::string(theme_.name) + " SECTOR", pal::TextDim);
            break;
        case GameState::RoundComplete:
            banner(-8, challengeBonusShown_ > 0 ? 34 : (luckyBonusShown_ > 0 ? 24 : 12));
            renderer_.textCentered(cx, VIEW_Y + VIEW_H / 2 - 4,
                                   round_.isChallenge()
                                       ? (challengePerfect_ ? "PERFECT" : "STAGE END")
                                       : "ROUND CLEAR",
                                   pal::Accent);
            if (challengeBonusShown_ > 0)
                renderer_.textCentered(cx, VIEW_Y + VIEW_H / 2 + 18,
                                       "BONUS " + std::to_string(challengeBonusShown_),
                                       pal::Accent);
            if (luckyBonusShown_ > 0)
                renderer_.textCentered(cx, VIEW_Y + VIEW_H / 2 + 8,
                                       "LUCKY BONUS " + std::to_string(luckyBonusShown_),
                                       pal::FlagLucky);
            break;

        case GameState::ChallengingStage:
            if (round_.chaseActive() && (tick_ / 12) % 2 == 0) {
                banner(-8, 12);
                renderer_.textCentered(cx, VIEW_Y + VIEW_H / 2 - 4, "OUT OF FUEL", pal::Danger);
            }
            break;

        case GameState::PlayerDeath: {
            banner(-8, 12);
            const char* why = "CRASH";
            if (deathCause_ == DeathCause::OutOfFuel) why = "OUT OF FUEL";
            if (deathCause_ == DeathCause::Rock)      why = "ROCK";
            if (deathCause_ == DeathCause::Enemy)     why = "CRASH";
            renderer_.textCentered(cx, VIEW_Y + VIEW_H / 2 - 4, why, pal::Danger);
            break;
        }
        case GameState::GameOver:
            banner(-16, 40);
            renderer_.textCentered(cx, VIEW_Y + VIEW_H / 2 - 12, "GAME OVER", pal::Danger);
            renderer_.textCentered(cx, VIEW_Y + VIEW_H / 2 + 4,
                                   "SCORE " + padNumber(score_.score(), 6), pal::Text);
            if (score_.score() >= score_.highScore() && score_.score() > 0 &&
                (tick_ / 20) % 2 == 0)
                renderer_.textCentered(cx, VIEW_Y + VIEW_H / 2 + 18, "NEW RECORD", pal::Accent);
            break;
        default:
            break;
    }
}

void Game::renderDebug() {
    const auto& p = round_.player();
    char buf[128];
    int y = 2;
    auto line = [&](const char* s) { renderer_.text(2, y, s, pal::FlagSpecial); y += 8; };

    std::snprintf(buf, sizeof buf, "FPS %d", static_cast<int>(fps_ + 0.5f)); line(buf);
    std::snprintf(buf, sizeof buf, "STATE %s", stateName(state_));          line(buf);
    std::snprintf(buf, sizeof buf, "ROUND %d", roundNumber_);               line(buf);
    std::snprintf(buf, sizeof buf, "SCORE %d", score_.score());             line(buf);
    std::snprintf(buf, sizeof buf, "LIVES %d", lives_.lives());             line(buf);
    std::snprintf(buf, sizeof buf, "FUEL %d", static_cast<int>(round_.fuel().fuel())); line(buf);
    std::snprintf(buf, sizeof buf, "SMOKE %d", round_.smoke().activeCount()); line(buf);
    std::snprintf(buf, sizeof buf, "ENEMIES %d", static_cast<int>(round_.enemies().size())); line(buf);
    std::snprintf(buf, sizeof buf, "POS %d %d",
                  static_cast<int>(p.position().x), static_cast<int>(p.position().y)); line(buf);
    std::snprintf(buf, sizeof buf, "DIR %s", dirName(p.direction()));       line(buf);
    std::snprintf(buf, sizeof buf, "FLAGS %d/%d",
                  round_.flagsCollected(), round_.flagsRequired());         line(buf);
    std::snprintf(buf, sizeof buf, "MULT X%d", score_.multiplier());        line(buf);
    line("F1 BOX F2 NAV F3 TGT F4 XY");
    line("F5 RADAR F6 FUEL F7 FREEZE");
    line("F8 FLAGS F9 RESTART");
}

} // namespace rx
