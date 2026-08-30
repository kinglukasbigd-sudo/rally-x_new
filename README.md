# New Rally-X — Phase 1

A clean-room reconstruction of the 1981 Namco arcade game *New Rally-X*, in C++17 and SDL2.

The goal of Phase 1 is faithfulness, not reinterpretation: a title screen, the maze, the
flags, the fuel, the smoke screen, the pursuing cars and the radar, behaving the way the
original does. There is no shop, no customisation, no progression system, no modern
control scheme and no futuristic UI — those are explicitly out of scope.

## Clean-room statement

Nothing here is derived from the original game's code or data. No ROM was disassembled or
consulted, no sprites, music or sound effects were extracted, and no copyrighted asset of
the original is redistributed. All artwork is original character-grid pixel art defined in
`src/rendering/SpriteRenderer.cpp` and `src/rendering/Font.h`; every sound effect is
synthesised at run time from square waves and noise in `src/audio/AudioManager.cpp`. The
reconstruction targets observable *gameplay behaviour and structure* only.

The assets that are not generated are the background music loops under `assets/audio/`,
built from tracks supplied by the project owner -- see **Background music** below. They
have nothing to do with the original game, but they are also not covered by the clean-room
statement: shipping them anywhere is the supplier's call, and needs whatever rights come
with those tracks.

## Building

Requires a C++17 compiler and SDL2 development headers.

```bash
make
```

```bash
make run
```

```bash
make test
```

If SDL2 headers are not installed and you cannot install packages, stage them locally:

```bash
./tools/fetch_sdl2.sh
```

`make BUILD=debug` builds with `-O0 -g` and the address/UB sanitizers. `CMakeLists.txt` is
provided as an alternative, but the Makefile is the build path that has actually been
exercised here.

### Command line

| Flag | Meaning |
| --- | --- |
| `--scale N` | Window scale factor (default 3, so 864×672) |
| `--data DIR` | Level data directory (default `levels`) |
| `--touch` | Enable touch controls (always on for Android) |
| `--touch-swipe` | Touch controls as swipe + 2-second hold (default) |
| `--touch-pad` | Touch controls as an on-screen four-way pad |
| `--windowed` | Run in a window instead of fullscreen |
| `--fullscreen` | Start fullscreen (the default) |
| `--round N` | Dev only: start at round N (handy for checking later layouts) |
| `--capture DIR` | Dev only: run a scripted demo headless and dump BMP frames |

## Android

The Android app runs the same C++ game core -- there is no Android-specific gameplay code
and no second copy of the levels. `android/app/src/main/cpp/CMakeLists.txt` compiles
`src/**` straight out of the repository root, and Gradle copies `levels/*.lvl` into the
APK's assets at build time. SDL's RWops layer resolves those paths to APK assets on Android
and to ordinary files on desktop, so `LevelLoader` is identical on both.

```bash
cd android && ./gradlew assembleDebug
```

```bash
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
```

Requirements: JDK 17, Android SDK platform 34, build-tools 35, NDK 27, SDK CMake 3.22.1,
and the SDL2 **source** tree (the desktop build only needs headers):

```bash
./tools/fetch_sdl2_android.sh
```

Point `android/local.properties` at your SDK (`sdk.dir=/path/to/Android/Sdk`). The build
produces native libraries for `arm64-v8a`, `armeabi-v7a`, `x86` and `x86_64`.

The app runs fullscreen in either orientation, with the playfield scaled as large as it
will go. The 288x224 aspect ratio is always preserved -- the picture is never stretched --
so a landscape phone keeps black bars at the sides and a portrait one shows the picture as
a band across the middle with the controls underneath.

Native libraries are built with 16 KB page alignment (`ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES`)
and packaged uncompressed, which Android 15+ requires; without it the loader warns on every
launch. Use `assembleRelease` rather than `assembleDebug` for anything you actually play:
a debuggable build triggers a compatibility warning of its own.

### Touch controls

Two schemes, switchable from the title screen (`[ CONTROLS: ... ]`), or on desktop with
`--touch-swipe` / `--touch-pad`:

**Swipe** (default) — flick in a direction to steer; the car keeps going that way until the
next flick, exactly as it would under a held joystick. **Tap anywhere to lay smoke**: there
is no button on screen, so the tap is the button. Holding for two seconds also fires, and a
square fills up at the touch point while it charges, so a deliberate hold is visible rather
than guessed at. A flick never costs smoke, so steering is free. Nothing is drawn over the
playfield, which is why the picture can use the whole screen.

A tap means different things depending on where the game is: during a round it is the smoke
button, and anywhere else -- title screen, game over -- it is Start.

**Pad** — an on-screen four-way pad and a smoke button. In landscape they sit in the empty
space either side of the picture; in portrait they move to the band underneath it; and on a
screen with no spare room either way they fall back to overlaying the bottom corners. The
layout follows the device live, so rotating mid-game just works.

Both orientations are supported and follow the device's own rotation lock.

Fingers are tracked individually in both schemes, so one can steer while another lays smoke.
A tap that is neither a flick nor a hold acts as Start. Both schemes emit the same `Action`
values the keyboard does, so `Player` and the rest of the game never learn that touch exists.

Note that `SDL_HINT_TOUCH_MOUSE_EVENTS` is turned off: SDL synthesises mouse clicks from
touches by default, which makes a phone deliver every gesture twice.

## Controls

| Input | Action |
| --- | --- |
| Arrow keys / WASD | Drive |
| Space / Left Ctrl | Smoke screen (costs fuel) |
| Enter / 1 | Start |
| P | Pause flag (reserved) |
| Esc | Quit |
| F12 | Toggle the debug overlay |

Debug toggles, available once the overlay is on: `F1` collision boxes, `F2` navigation
graph, `F3` enemy targets, `F4` player coordinates, `F5` radar maze underlay, `F6` infinite
fuel, `F7` freeze enemies, `F8` bank every flag, `F9` restart the round.

## How it plays

Collect all ten flags in the maze to clear a round. **The car never stops.** Steering
always wins, but if you drive into a wall without steering somewhere legal, the car picks a
random open direction and keeps going — in a dead end it turns around. (This is a
deliberate departure from the 1981 original, where the car parks against the wall.)
 Flags score 100, 200, … up to 1000 in
the order they are taken, and the sequence restarts each round. The **Special Flag** scores
nothing itself but doubles every flag taken after it, until the round ends or a car is
lost. The **Lucky Flag** pays a bonus scaled by the fuel left in the tank, so it is worth
most when taken early.

Every pursuit car in a round starts in the **enemy pen** — a single straight lane of six or
seven tiles marked out in the level data, always at the far end of the maze from where you
start (16+ tiles away through the corridors, typically 24). The whole pack sits there
together, launches on the same step after a short countdown, and only then fans out to hunt
you. You always know where they are coming from and you always get a head start.

Every round is a different maze, in a different colour, with a different pursuit. There are
twelve authored rounds and eight colour schemes; the ramp adds cars, speed and rocks while
raising the burn rate. The tank is always 100 units, in every round: difficulty comes from
how fast it burns, never from starting with less, so the gauge always begins full and only
the clock behind it speeds up. That runs 1.00 units/sec in round 1 to 1.63 in round 12 --
100 seconds down to 61 -- and the challenging stages burn far harder still (2.00 to 2.58,
about 50 down to 39 seconds). The maze grid itself never changes -- every
round is 32x32 tiles with one-tile roads -- but the wall blocks get smaller and more
numerous in the later rounds (about 37-44 blocks averaging 10 tiles early on, 47-53
averaging 8 later), so the layouts break up into more, tighter obstacles. Rounds
past the twelfth cycle back round with another difficulty step and the next colour scheme.

Fuel drains continuously and drops sharply while the smoke screen is running; an empty tank
kills the car, as does a rock or a red pursuit car. Each press of the smoke button lays a
short burst of three puffs and stops -- holding the button does nothing further, so smoke
can never run away with the tank. The smoke stalls any pursuit car that drives into it for
a few seconds. Pursuit cars cannot drive through rocks either --
they are blind to them, so they run into one, bounce off, and go around. Every third round is a **Challenging Stage**: no
pursuit cars at all, a race against the fuel gauge, a perfect bonus for clearing every flag,
and no car lost if the run ends early.

## Layout

```
src/
  core/        Game (state machine + fixed-step loop), InputManager, TouchControls,
               FileSystem, Types, Debug
  world/       TileMap, Maze, LevelLoader, Camera, CollisionSystem
  entities/    Player, Enemy, Flag, Rock, SmokeCloud
  ai/          NavigationGraph, Pathfinding, EnemyAI
  gameplay/    Round, ScoreSystem, FuelSystem, LifeSystem, SmokeSystem,
               ChallengeStage, LuckyFlagBonusCalculator
  rendering/   Renderer, SpriteRenderer, HUD, Radar, Palette, Font, RoundTheme
  audio/       AudioManager (synth effects + music mixer)
levels/        level01..level12.lvl  (external data, no maze is hard-coded)
assets/audio/  music_normal.wav, music_challenge.wav -- the looping background tracks
android/       Gradle + NDK project wrapping the same core (see Android, above)
tools/         genlevel.py, find_loop.py, make_music.py, fetch_sdl2.sh,
               fetch_sdl2_android.sh
tests/         128 gameplay tests, run with `make test`
```

Presentation runs at a fixed internal resolution of 288×224, drawn into an offscreen
framebuffer and scaled up with nearest-neighbour. A window uses whole-number scaling so
every game pixel stays exactly square; fullscreen scales to the screen edge instead, holding
the aspect ratio. The maze is
32×32 tiles of 16px (512×512 world pixels) scrolling inside a 208×208 viewport, with a
20px score strip along the top and the radar, fuel gauge and car count down the right.

Per-round colours live in `rendering/RoundTheme.cpp` as a flat table of eight schemes. Only
the maze blocks and the pursuit cars are re-coloured — the HUD, the player car and the flags
keep fixed colours, because those are what the player reads the game by.

Simulation runs on a fixed 1/60 s timestep with an accumulator, so movement, collision,
fuel, AI, smoke and animation are all independent of the render frame rate.

## Background music

Ordinary rounds and challenging stages each play their own looping track underneath the
sound effects, so a bonus stage sounds like the change of pace it is. The title screen and
the between-rounds cards stay quiet, so nothing competes with the effects there.

| Track | File | Plays during |
| --- | --- | --- |
| Normal | `assets/audio/music_normal.wav` | ordinary rounds |
| Challenge | `assets/audio/music_challenge.wav` | challenging stages |

The loop is plain 22050 Hz mono 16-bit PCM -- the exact format the audio device already
runs at, so nothing is resampled at run time -- and it is mixed in at
`AudioManager::MUSIC_GAIN` (0.15), which puts it comfortably under the effects. Change that
one constant to make it louder or quieter.

There are two tools for building a loop; both need `ffmpeg` on the path (or
`FFMPEG=/path/to/ffmpeg`).

**`tools/find_loop.py`** analyses the track and picks the loop point for you. Prefer it:

```bash
tools/find_loop.py <input.mp3> assets/audio/music_normal.wav --min=12 --max=28
```

It estimates the tempo *and* the downbeat, restricts the loop to a whole number of bars so
the rhythm never stutters across the join, scores candidates on spectral continuity and
loudness match, snaps both ends to a rising zero crossing, and sizes the crossfade to how
well the ends actually match -- a few milliseconds when they do, around 600ms when they do
not. It also reports whether a *sample-exact* loop is possible at all, which it only is if
the source literally repeats a section. Most through-composed tracks do not, and the tool
says so rather than pretending.

**`tools/make_music.py`** is the blunt version: take the last N seconds (or `all`), trim any
fade-out, crossfade, normalise.

```bash
tools/make_music.py <input.mp3> assets/audio/music_challenge.wav all
```

`AudioManager` mixes on an SDL audio callback: the current music loop plus up to eight
overlapping effect voices. It used to queue effects one after another, which cannot play
music and effects at the same time.

## Level data

Levels are plain text (`levels/*.lvl`) — nothing about a maze lives in C++:

```
name ROUND 1
type NORMAL          # or CHALLENGE
difficulty 1
fuel 100             # tank capacity -- always 100; balance the drain instead
fuelDrain 1.00       # units per second, so the round lasts fuel/fuelDrain
playerSpeed 1.35     # world pixels per 1/60s step
enemySpeed 1.05
enemies 3            # cars to place in the pen; clamped to the pen length
maze
################################
#..............................#
...
```

Maze legend: `.` road, `#` wall, `R` rock, `F` flag, `S` special flag, `L` lucky flag,
`P` player spawn, `E` enemy pen. The `E` tiles must form one contiguous straight run — the
generator picks the farthest such run from the player spawn, and `make test` enforces both
the shape and the distance on every shipped level. Regenerate with `make levels`. Rounds past the last
authored level cycle back round with a difficulty step added each lap.

## Reconstructed behaviour and assumptions

Where the original's exact numbers are not observable from the outside, the behaviour is
implemented in an isolated, clearly-named place so it can be retuned without touching
gameplay code. The judgement calls made for this phase:

- **Lucky Flag payout** — linear in remaining fuel from 100 to 2000, snapped to hundreds.
  Isolated in `LuckyFlagBonusCalculator`.
- **Special Flag lifetime** — the ×2 multiplier persists for the rest of the round and is
  cleared when a car is lost.
- **Challenging Stage** — no cars are placed, a perfect clear pays 5000, and ending early
  (dry tank or a rock) does not cost a life. Isolated in `ChallengeStage`.
- **Bonus car** — one extra car at 20,000 points.
- **Losing a life** — requested house rule, not original: the flags already collected stay
  collected, and the scoring ladder carries on where it left off. Only the car, the pursuit
  and the tank are reset (`Round::restartAfterDeath`). A new round resets everything.
- **Smoke** — requested house rule, not original: one press lays exactly three puffs
  (`SmokeSystem::PUFFS_PER_BURST`) instead of running continuously while held.
- **Wall behaviour** — requested house rule, not original: the car redirects at random
  rather than stopping. `Player::chooseEscapeDirection`.
- **Rocks and pursuit cars** — requested house rule, not original: rocks are solid to the
  cars. They navigate as if the rocks were not there, so they nose into one, recoil, and
  refuse that direction for a moment (`Round::buildEnemyMap`, `EnemyAI::escapeFrom`). The
  player still *dies* on a rock rather than being blocked by it.
- **Enemy launch** — requested house rule, not original: the whole pack starts in one pen
  far from the player and launches simultaneously after a 1.4s countdown, rather than
  trickling in from scattered points. `Round::spawnEnemies`.
- **Enemy pursuit** — cars take the turn that shortens the gap at long range and route
  properly through the maze once inside about a dozen tiles, with a small random turn
  chance so the pack does not converge into single file. Deliberately shallow: no
  prediction, no coordination, no flanking.
