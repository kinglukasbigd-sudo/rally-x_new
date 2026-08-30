#include "TestFramework.h"
#include "core/Types.h"
#include "rendering/Font.h"
#include "rendering/RoundTheme.h"
#include "entities/Player.h"
#include "gameplay/Round.h"
#include "gameplay/ScoreSystem.h"
#include "core/InputManager.h"
#include "core/TouchControls.h"
#include "world/LevelLoader.h"
#include <set>
#include <cstring>

using namespace rx;

// ---------------------------------------------------------------------------
// Score strip geometry: the digits used to be clipped by the divider line.
// ---------------------------------------------------------------------------

TEST(the_score_strip_has_room_for_both_text_rows_and_the_divider) {
    const int labelTop  = 2,  labelBottom  = labelTop + FONT_H - 1;    // "1UP"
    const int digitTop  = 11, digitBottom  = digitTop + FONT_H - 1;    // "000000"
    const int dividerY  = VIEW_Y - 1;

    CHECK(labelBottom < digitTop);        // rows do not overlap each other
    CHECK(digitBottom < dividerY);        // the divider clears the digits
    CHECK(dividerY < VIEW_Y);             // and stays out of the playfield
    CHECK(digitBottom < VIEW_Y);          // nothing is cut off by the maze
}

TEST(the_viewport_still_fills_the_screen_below_the_strip) {
    CHECK_EQ(VIEW_Y + VIEW_H, SCREEN_H);
    CHECK_EQ(VIEW_X + VIEW_W, PANEL_X);
    CHECK(RADAR_Y >= VIEW_Y);
    CHECK(RADAR_Y + RADAR_H < SCREEN_H);
    CHECK(RADAR_X + RADAR_W <= SCREEN_W);
}

// ---------------------------------------------------------------------------
// House rule: a wall redirects the car instead of parking it.
// ---------------------------------------------------------------------------

namespace {

// A T-junction: driving right dead-ends, with up and down both open.
const char* kTee =
    "name TEE\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 2\nmaze\n"
    "#######\n"
    "###.###\n"
    "###.###\n"
    "#P...##\n"
    "###.###\n"
    "###.###\n"
    "#######\n";

const char* kBlindAlley =
    "name ALLEY\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 2\nmaze\n"
    "######\n"
    "#P...#\n"
    "######\n";

LevelData parse(const char* t) {
    LevelData d;
    CHECK(LevelLoader::loadFromString(t, d));
    return d;
}

void hold(InputManager& in, Action a) {
    for (Action x : { Action::Up, Action::Down, Action::Left, Action::Right })
        in.setFromExternal(x, x == a);
}

} // namespace

TEST(the_car_does_not_stop_when_it_runs_into_a_wall) {
    LevelData d = parse(kTee);
    Player p;
    p.spawn(Vec2{1 * TILE + 8.f, 3 * TILE + 8.f}, Direction::Right, 2.f);

    // Drive right into the dead end; the car must turn rather than park, and
    // must still be moving on the very step the wall was reached.
    bool redirected = false;
    for (int i = 0; i < 200 && !redirected; ++i) {
        p.update(d.map, Direction::None, static_cast<float>(FIXED_DT));
        redirected = p.autoTurnedThisStep();
    }
    CHECK(redirected);
    CHECK(p.moving());
    CHECK(p.direction() != Direction::Right);
    CHECK(!d.map.isWallAtPixel(p.position().x, p.position().y));
}

TEST(a_wall_redirect_picks_an_open_direction) {
    LevelData d = parse(kTee);
    Player p;
    p.spawn(Vec2{1 * TILE + 8.f, 3 * TILE + 8.f}, Direction::Right, 2.f);

    bool sawRedirect = false;
    for (int i = 0; i < 400; ++i) {
        p.update(d.map, Direction::None, static_cast<float>(FIXED_DT));
        if (p.autoTurnedThisStep()) sawRedirect = true;
        // Whatever it chooses, it must never end up inside the maze walls.
        CHECK(!d.map.isWallAtPixel(p.position().x, p.position().y));
    }
    CHECK(sawRedirect);
}

TEST(a_dead_end_turns_the_car_around) {
    LevelData d = parse(kBlindAlley);
    Player p;
    p.spawn(Vec2{1 * TILE + 8.f, 1 * TILE + 8.f}, Direction::Right, 2.f);

    // A one-tile-tall alley: the only escape from the far wall is a U-turn,
    // so the first redirect must be exactly that.
    bool redirected = false;
    for (int i = 0; i < 120 && !redirected; ++i) {
        p.update(d.map, Direction::None, static_cast<float>(FIXED_DT));
        redirected = p.autoTurnedThisStep();
    }
    CHECK(redirected);
    CHECK(p.direction() == Direction::Left);
    CHECK(p.moving());
}

TEST(steering_still_beats_the_automatic_redirect) {
    LevelData d = parse(kTee);
    Player p;
    p.spawn(Vec2{1 * TILE + 8.f, 3 * TILE + 8.f}, Direction::Right, 2.f);

    // Hold Up at the junction: the player's choice must be taken, not a
    // random one, and no redirect should fire.
    for (int i = 0; i < 40; ++i) p.update(d.map, Direction::Up, static_cast<float>(FIXED_DT));
    CHECK(p.direction() == Direction::Up);
    CHECK(!p.autoTurnedThisStep());
}

TEST(the_car_keeps_driving_forever_in_a_real_maze) {
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level01.lvl", d));
    d.fuelDrain = 0.f;
    d.rocks.clear();            // rocks would legitimately end the run
    d.enemyPen.clear();
    d.enemyCount = 0;
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    hold(in, Action::Right);    // one direction held the whole time

    int stalledFrames = 0;
    for (int i = 0; i < 60 * 30; ++i) {
        r.update(in, s, static_cast<float>(FIXED_DT));
        if (!r.player().moving()) ++stalledFrames;
        CHECK(!r.map().isWallAtPixel(r.player().position().x, r.player().position().y));
    }
    CHECK_EQ(stalledFrames, 0);   // never once parked against a wall
}

// ---------------------------------------------------------------------------
// Per-round themes.
// ---------------------------------------------------------------------------

namespace {
bool sameColor(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}
} // namespace

TEST(consecutive_rounds_look_different) {
    for (int round = 1; round < ROUND_THEME_COUNT; ++round) {
        const RoundTheme a = themeFor(round);
        const RoundTheme b = themeFor(round + 1);
        CHECK(!sameColor(a.wall, b.wall));
        CHECK(!sameColor(a.enemyBody, b.enemyBody));
    }
}

TEST(every_theme_is_a_distinct_colour_scheme) {
    std::set<uint32_t> walls, cars;
    for (int i = 0; i < ROUND_THEME_COUNT; ++i) {
        const RoundTheme& t = themeByIndex(i);
        walls.insert((t.wall.r << 16) | (t.wall.g << 8) | t.wall.b);
        cars.insert((t.enemyBody.r << 16) | (t.enemyBody.g << 8) | t.enemyBody.b);

        // Wall shading must stay readable: light edge, body, dark edge.
        CHECK(t.wallLight.r + t.wallLight.g + t.wallLight.b >
              t.wall.r + t.wall.g + t.wall.b);
        CHECK(t.wallDark.r + t.wallDark.g + t.wallDark.b <
              t.wall.r + t.wall.g + t.wall.b);

        // A pursuit car must never be mistakable for a maze wall.
        CHECK(!sameColor(t.enemyBody, t.wall));
        CHECK(t.name[0] != '\0');
    }
    CHECK_EQ(static_cast<int>(walls.size()), ROUND_THEME_COUNT);
    CHECK_EQ(static_cast<int>(cars.size()),  ROUND_THEME_COUNT);
}

TEST(themes_cycle_and_never_index_out_of_range) {
    CHECK_EQ(themeFor(1).index, 0);
    CHECK_EQ(themeFor(ROUND_THEME_COUNT + 1).index, 0);
    CHECK_EQ(themeFor(0).index, 0);        // clamped, not negative
    CHECK_EQ(themeFor(-5).index, 0);
    CHECK_EQ(themeByIndex(ROUND_THEME_COUNT * 3 + 2).index, 2);
}

// ---------------------------------------------------------------------------
// On-screen controls.  The emulator can only inject one gesture at a time, so
// the multi-finger behaviour is pinned down here instead.
// ---------------------------------------------------------------------------

namespace {

// A phone-shaped window: 2340x1080 with the 288x224 playfield centred.
constexpr int PHONE_W = 2340, PHONE_H = 1080, PHONE_SCALE = 4;

Rect phoneGameRect() {
    return Rect{ static_cast<float>((PHONE_W - SCREEN_W * PHONE_SCALE) / 2),
                 static_cast<float>((PHONE_H - SCREEN_H * PHONE_SCALE) / 2),
                 static_cast<float>(SCREEN_W * PHONE_SCALE),
                 static_cast<float>(SCREEN_H * PHONE_SCALE) };
}

TouchControls phoneLayout(TouchScheme scheme = TouchScheme::Pad) {
    TouchControls t;
    t.setEnabled(true);
    t.setScheme(scheme);
    t.layout(PHONE_W, PHONE_H, phoneGameRect());
    return t;
}

Vec2 centreOf(const Rect& r) { return Vec2{ r.x + r.w * 0.5f, r.y + r.h * 0.5f }; }

} // namespace

TEST(the_pad_sits_beside_the_playfield_on_a_phone) {
    TouchControls t = phoneLayout();
    CHECK(!t.overlaid());

    const int scale = 4;
    const float gameL = (2340 - SCREEN_W * scale) / 2.f;
    const float gameR = gameL + SCREEN_W * scale;

    // Nothing may cover the maze.
    for (Action a : { Action::Up, Action::Down, Action::Left, Action::Right }) {
        const Rect& r = t.rect(a);
        CHECK(r.w > 0.f);
        CHECK(r.x + r.w <= gameL + 0.5f);
    }
    const Rect& s = t.rect(Action::Smoke);
    CHECK(s.x >= gameR - 0.5f);
    CHECK(s.x + s.w <= 2340.f);
}

TEST(the_pad_falls_back_to_an_overlay_on_a_square_screen) {
    TouchControls t;
    t.setEnabled(true);
    t.setScheme(TouchScheme::Pad);
    // A 4:3 window leaves no margin worth using.
    t.layout(288 * 3, 224 * 3, Rect{ 0.f, 0.f, 288.f * 3, 224.f * 3 });
    CHECK(t.overlaid());
    CHECK(t.rect(Action::Up).w > 0.f);
    CHECK(t.rect(Action::Smoke).w > 0.f);
}

TEST(a_finger_on_a_button_presses_only_that_button) {
    TouchControls t = phoneLayout();
    const Vec2 up = centreOf(t.rect(Action::Up));
    t.fingerDown(1, up.x, up.y);

    CHECK(t.pressed(Action::Up));
    CHECK(!t.pressed(Action::Down));
    CHECK(!t.pressed(Action::Left));
    CHECK(!t.pressed(Action::Right));
    CHECK(!t.pressed(Action::Smoke));

    t.fingerUp(1);
    CHECK(!t.pressed(Action::Up));
}

TEST(steering_and_smoke_work_at_the_same_time) {
    TouchControls t = phoneLayout();
    const Vec2 left  = centreOf(t.rect(Action::Left));
    const Vec2 smoke = centreOf(t.rect(Action::Smoke));

    t.fingerDown(7, left.x, left.y);
    t.fingerDown(9, smoke.x, smoke.y);
    CHECK(t.pressed(Action::Left));
    CHECK(t.pressed(Action::Smoke));

    // Both reach the shared action set, so Player sees exactly what the
    // keyboard would have produced.
    InputManager in;
    t.applyTo(in);
    CHECK(in.down(Action::Left));
    CHECK(in.down(Action::Smoke));
    CHECK(in.desiredDirection() == Direction::Left);

    // Lifting one finger must not release the other.
    t.fingerUp(7);
    CHECK(!t.pressed(Action::Left));
    CHECK(t.pressed(Action::Smoke));
}

TEST(sliding_a_finger_between_buttons_hands_over_cleanly) {
    TouchControls t = phoneLayout();
    const Vec2 up   = centreOf(t.rect(Action::Up));
    const Vec2 down = centreOf(t.rect(Action::Down));

    t.fingerDown(3, up.x, up.y);
    CHECK(t.pressed(Action::Up));

    t.fingerMove(3, down.x, down.y);
    CHECK(!t.pressed(Action::Up));
    CHECK(t.pressed(Action::Down));

    // Sliding off the pad entirely releases everything that finger held.
    t.fingerMove(3, 5.f, 5.f);
    CHECK(!t.pressed(Action::Down));
}

TEST(losing_focus_releases_every_finger) {
    TouchControls t = phoneLayout();
    t.fingerDown(1, centreOf(t.rect(Action::Right)).x, centreOf(t.rect(Action::Right)).y);
    t.fingerDown(2, centreOf(t.rect(Action::Smoke)).x, centreOf(t.rect(Action::Smoke)).y);
    CHECK(t.pressed(Action::Right));
    CHECK(t.pressed(Action::Smoke));

    // Backgrounding the app must not leave the car driving into a wall forever.
    t.releaseAll();
    for (Action a : { Action::Up, Action::Down, Action::Left, Action::Right, Action::Smoke })
        CHECK(!t.pressed(a));
}

TEST(a_tap_away_from_the_pad_is_not_a_control_press) {
    TouchControls t = phoneLayout();
    // The middle of the playfield is where "tap to start" lands.
    CHECK(t.hitTest(2340 / 2.f, 1080 / 2.f) == Action::Count);
    CHECK(t.hitTest(centreOf(t.rect(Action::Up)).x,
                    centreOf(t.rect(Action::Up)).y) == Action::Up);
}

TEST(a_disabled_pad_never_touches_the_action_set) {
    TouchControls t = phoneLayout();
    t.fingerDown(1, centreOf(t.rect(Action::Up)).x, centreOf(t.rect(Action::Up)).y);
    t.setEnabled(false);

    InputManager in;
    t.applyTo(in);
    CHECK(!in.down(Action::Up));
}

TEST(the_font_covers_every_character_the_game_prints) {
    // Any caption using a glyph the font lacks renders as a blank gap, which
    // is how "[ CONTROLS: SWIPE ]" lost its brackets.
    const char* used =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,-:/!?*()=+<>'[]";
    for (const char* c = used; *c; ++c) {
        bool found = false;
        for (int i = 0; i < FONT_GLYPH_COUNT; ++i)
            if (FONT_GLYPHS[i].c == *c) { found = true; break; }
        if (!found)
            ::test::fail(__FILE__, __LINE__,
                         std::string("font has no glyph for '") + *c + "'");
    }
}

TEST(every_glyph_is_the_declared_size) {
    for (int i = 0; i < FONT_GLYPH_COUNT; ++i)
        for (int row = 0; row < FONT_H; ++row)
            CHECK_EQ(static_cast<int>(std::strlen(FONT_GLYPHS[i].rows[row])), FONT_W);
}

// ---------------------------------------------------------------------------
// Swipe scheme: flick to steer, press and hold to lay smoke.
// ---------------------------------------------------------------------------

namespace {

TouchControls swipePad() { return phoneLayout(TouchScheme::Swipe); }

// Far enough to clear the flick threshold on this screen.
constexpr float FAR = 0.06f * PHONE_H;

void holdFor(TouchControls& t, float seconds) {
    const int steps = static_cast<int>(seconds / FIXED_DT);
    for (int i = 0; i < steps; ++i) t.update(static_cast<float>(FIXED_DT));
}

} // namespace

TEST(the_swipe_scheme_draws_nothing_over_the_playfield) {
    TouchControls t = swipePad();
    // No pad rectangles at all, so the picture can use the whole screen.
    for (Action a : { Action::Up, Action::Down, Action::Left, Action::Right, Action::Smoke })
        CHECK_EQ(static_cast<int>(t.rect(a).w), 0);
    CHECK(!t.overlaid());
    CHECK(t.hitTest(PHONE_W / 2.f, PHONE_H / 2.f) == Action::Count);
}

TEST(a_flick_sets_the_direction_it_was_flicked_in) {
    struct Case { float dx, dy; Direction want; };
    const Case cases[] = {
        { 0.f, -FAR, Direction::Up    },
        { 0.f,  FAR, Direction::Down  },
        { -FAR, 0.f, Direction::Left  },
        {  FAR, 0.f, Direction::Right },
    };
    for (const Case& c : cases) {
        TouchControls t = swipePad();
        const float ox = PHONE_W / 2.f, oy = PHONE_H / 2.f;
        t.fingerDown(1, ox, oy);
        t.fingerMove(1, ox + c.dx, oy + c.dy);
        CHECK(t.steering() == c.want);
        t.fingerUp(1);
        // Steering persists after the finger lifts: the car keeps driving.
        CHECK(t.steering() == c.want);
    }
}

TEST(a_diagonal_flick_resolves_to_its_dominant_axis) {
    TouchControls t = swipePad();
    const float ox = PHONE_W / 2.f, oy = PHONE_H / 2.f;
    // Mostly rightwards, slightly up: there are no diagonals in this maze.
    t.fingerDown(1, ox, oy);
    t.fingerMove(1, ox + FAR, oy - FAR * 0.4f);
    CHECK(t.steering() == Direction::Right);
}

TEST(a_short_drag_is_not_a_flick) {
    TouchControls t = swipePad();
    const float ox = PHONE_W / 2.f, oy = PHONE_H / 2.f;
    t.fingerDown(1, ox, oy);
    t.fingerMove(1, ox + 4.f, oy);
    CHECK(t.steering() == Direction::None);
}

TEST(a_second_flick_in_one_drag_changes_direction) {
    TouchControls t = swipePad();
    float x = PHONE_W / 2.f;
    const float y = PHONE_H / 2.f;
    t.fingerDown(1, x, y);
    t.fingerMove(1, x += FAR, y);
    CHECK(t.steering() == Direction::Right);
    t.fingerMove(1, x, y + FAR);
    CHECK(t.steering() == Direction::Down);
}

TEST(holding_for_two_seconds_lays_smoke) {
    TouchControls t = swipePad();
    t.fingerDown(1, PHONE_W / 2.f, PHONE_H / 2.f);

    holdFor(t, TouchControls::HOLD_TO_SMOKE - 0.3f);
    CHECK(!t.smoking());
    CHECK(t.holdCharging());
    CHECK(t.holdProgress() > 0.5f && t.holdProgress() < 1.0f);

    holdFor(t, 0.5f);
    CHECK(t.smoking());
    CHECK(!t.holdCharging());          // charged, not charging

    InputManager in;
    t.applyTo(in);
    CHECK(in.down(Action::Smoke));

    t.fingerUp(1);
    CHECK(!t.smoking());               // releasing stops the smoke
}

TEST(a_flick_never_becomes_a_hold) {
    TouchControls t = swipePad();
    const float ox = PHONE_W / 2.f, oy = PHONE_H / 2.f;
    t.fingerDown(1, ox, oy);
    t.fingerMove(1, ox + FAR, oy);     // this finger is now a flick

    holdFor(t, TouchControls::HOLD_TO_SMOKE + 1.0f);
    CHECK(!t.smoking());
    CHECK(t.steering() == Direction::Right);
}

TEST(one_finger_can_steer_while_another_lays_smoke) {
    TouchControls t = swipePad();

    // Finger 1 flicks left.
    t.fingerDown(1, 600.f, 500.f);
    t.fingerMove(1, 600.f - FAR, 500.f);
    CHECK(t.steering() == Direction::Left);
    t.fingerUp(1);

    // Finger 2 presses and holds somewhere else.
    t.fingerDown(2, 1800.f, 700.f);
    holdFor(t, TouchControls::HOLD_TO_SMOKE + 0.2f);

    CHECK(t.smoking());
    CHECK(t.steering() == Direction::Left);

    InputManager in;
    t.applyTo(in);
    CHECK(in.down(Action::Left));
    CHECK(in.down(Action::Smoke));
    CHECK(in.desiredDirection() == Direction::Left);
}

TEST(a_quick_tap_is_a_start_press_not_a_steer_or_a_smoke) {
    TouchControls t = swipePad();
    t.fingerDown(1, PHONE_W / 2.f, PHONE_H / 2.f);
    holdFor(t, 0.1f);
    t.fingerUp(1);

    CHECK(t.steering() == Direction::None);
    CHECK(!t.smoking());
    CHECK(t.consumeTap());
    CHECK(!t.consumeTap());            // consumed exactly once
}

TEST(a_long_press_is_not_also_a_tap) {
    TouchControls t = swipePad();
    t.fingerDown(1, PHONE_W / 2.f, PHONE_H / 2.f);
    holdFor(t, TouchControls::HOLD_TO_SMOKE + 0.2f);
    t.fingerUp(1);
    CHECK(!t.consumeTap());
}

TEST(switching_scheme_clears_whatever_was_held) {
    TouchControls t = swipePad();
    t.fingerDown(1, 600.f, 500.f);
    t.fingerMove(1, 600.f + FAR, 500.f);
    holdFor(t, TouchControls::HOLD_TO_SMOKE + 0.2f);
    CHECK(t.steering() == Direction::Right);

    t.setScheme(TouchScheme::Pad);
    CHECK(t.steering() == Direction::None);
    CHECK(!t.smoking());

    InputManager in;
    t.applyTo(in);
    CHECK(!in.down(Action::Right));
    CHECK(!in.down(Action::Smoke));
}

TEST(backgrounding_the_app_stops_the_smoke) {
    TouchControls t = swipePad();
    t.fingerDown(1, PHONE_W / 2.f, PHONE_H / 2.f);
    holdFor(t, TouchControls::HOLD_TO_SMOKE + 0.2f);
    CHECK(t.smoking());

    t.releaseAll();
    CHECK(!t.smoking());
    CHECK_NEAR(t.holdProgress(), 0.0, 1e-5);
}

// ---------------------------------------------------------------------------
// Portrait, and flag progress surviving a lost life.
// ---------------------------------------------------------------------------

TEST(the_pad_moves_below_the_picture_in_portrait) {
    TouchControls t;
    t.setEnabled(true);
    t.setScheme(TouchScheme::Pad);

    // A phone held upright: 1080x2340 with the 288x224 picture as a band
    // across the middle.
    const int w = 1080, h = 2340;
    const int scale = 3;                              // min(1080/288, 2340/224)
    const Rect game{ static_cast<float>((w - SCREEN_W * scale) / 2),
                     static_cast<float>((h - SCREEN_H * scale) / 2),
                     static_cast<float>(SCREEN_W * scale),
                     static_cast<float>(SCREEN_H * scale) };
    t.layout(w, h, game);

    CHECK(t.placement() == TouchControls::PadPlacement::Below);
    CHECK(!t.overlaid());

    // Every control sits under the picture, never on top of it, and inside
    // the screen.
    for (Action a : { Action::Up, Action::Down, Action::Left, Action::Right, Action::Smoke }) {
        const Rect& r = t.rect(a);
        CHECK(r.w > 0.f);
        CHECK(r.y >= game.y + game.h - 0.5f);
        CHECK(r.x >= 0.f);
        CHECK(r.x + r.w <= static_cast<float>(w));
        CHECK(r.y + r.h <= static_cast<float>(h));
    }
    // Pad on the left, smoke on the right.
    CHECK(t.rect(Action::Left).x < t.rect(Action::Smoke).x);
}

TEST(landscape_still_puts_the_pad_beside_the_picture) {
    TouchControls t = phoneLayout(TouchScheme::Pad);
    CHECK(t.placement() == TouchControls::PadPlacement::Sides);
}

TEST(portrait_swipe_still_works_and_draws_nothing) {
    TouchControls t;
    t.setEnabled(true);
    t.setScheme(TouchScheme::Swipe);
    const int w = 1080, h = 2340, scale = 3;
    t.layout(w, h, Rect{ (w - SCREEN_W * scale) / 2.f, (h - SCREEN_H * scale) / 2.f,
                         static_cast<float>(SCREEN_W * scale),
                         static_cast<float>(SCREEN_H * scale) });

    for (Action a : { Action::Up, Action::Down, Action::Left, Action::Right, Action::Smoke })
        CHECK_EQ(static_cast<int>(t.rect(a).w), 0);

    // A flick still steers, measured against the short (horizontal) edge.
    const float far = 0.06f * w;
    t.fingerDown(1, w / 2.f, h / 2.f);
    t.fingerMove(1, w / 2.f, h / 2.f - far);
    CHECK(t.steering() == Direction::Up);
}

// ---------------------------------------------------------------------------
// Touch actions must be released, not just pressed.
//
// A keyboard releases itself with a key-up event; touch has nothing
// equivalent, so an action written only on press stays down forever.  These
// go through the real TouchControls -> InputManager -> Round chain, because
// testing the pad in isolation is exactly what let this through the first time.
// ---------------------------------------------------------------------------

TEST(lifting_a_finger_releases_the_action) {
    TouchControls t = phoneLayout(TouchScheme::Pad);
    InputManager in;
    const Vec2 btn = centreOf(t.rect(Action::Smoke));

    t.fingerDown(1, btn.x, btn.y);
    in.beginFrame(); t.applyTo(in);
    CHECK(in.down(Action::Smoke));
    CHECK(in.pressed(Action::Smoke));

    t.fingerUp(1);
    in.beginFrame(); t.applyTo(in);
    CHECK(!in.down(Action::Smoke));      // must not latch on
    CHECK(in.released(Action::Smoke));
}

TEST(a_new_flick_releases_the_previous_direction) {
    TouchControls t = swipePad();
    InputManager in;
    const float ox = PHONE_W / 2.f, oy = PHONE_H / 2.f;

    t.fingerDown(1, ox, oy); t.fingerMove(1, ox + FAR, oy); t.fingerUp(1);
    in.beginFrame(); t.applyTo(in);
    CHECK(in.down(Action::Right));

    t.fingerDown(2, ox, oy); t.fingerMove(2, ox, oy - FAR); t.fingerUp(2);
    in.beginFrame(); t.applyTo(in);
    CHECK(in.down(Action::Up));
    CHECK(!in.down(Action::Right));      // the old heading let go
    CHECK(in.desiredDirection() == Direction::Up);
}

TEST(every_press_of_the_smoke_button_lays_a_fresh_burst) {
    LevelData d = parse(kTee);
    d.fuelDrain = 0.f;                   // isolate the smoke cost
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    TouchControls t = phoneLayout(TouchScheme::Pad);
    InputManager in;
    const Vec2 btn = centreOf(t.rect(Action::Smoke));

    auto frame = [&]() {
        in.beginFrame();
        t.applyTo(in);
        r.update(in, s, static_cast<float>(FIXED_DT));
    };

    constexpr int PRESSES = 4;
    for (int p = 0; p < PRESSES; ++p) {
        t.fingerDown(100 + p, btn.x, btn.y);
        for (int i = 0; i < 40; ++i) frame();     // hold a while
        t.fingerUp(100 + p);
        for (int i = 0; i < 10; ++i) frame();     // let go
    }

    // Four presses must cost four bursts, not one.
    const float used = r.fuel().capacity() - r.fuel().fuel();
    CHECK_NEAR(used, PRESSES * SmokeSystem::PUFFS_PER_BURST * SmokeSystem::FUEL_PER_PUFF, 0.01);
}

TEST(every_two_second_hold_lays_a_fresh_burst_in_swipe_mode) {
    LevelData d = parse(kTee);
    d.fuelDrain = 0.f;
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    TouchControls t = swipePad();
    InputManager in;

    auto frame = [&]() {
        t.update(static_cast<float>(FIXED_DT));
        in.beginFrame();
        t.applyTo(in);
        r.update(in, s, static_cast<float>(FIXED_DT));
    };

    constexpr int HOLDS = 3;
    for (int h = 0; h < HOLDS; ++h) {
        t.fingerDown(200 + h, PHONE_W / 2.f, PHONE_H / 2.f);
        for (int i = 0; i < 160; ++i) frame();    // ~2.7s, past the threshold
        t.fingerUp(200 + h);
        for (int i = 0; i < 20; ++i) frame();
    }

    const float used = r.fuel().capacity() - r.fuel().fuel();
    CHECK_NEAR(used, HOLDS * SmokeSystem::PUFFS_PER_BURST * SmokeSystem::FUEL_PER_PUFF, 0.01);
}

TEST(the_smoke_button_can_be_worked_indefinitely) {
    // The failure this guards against was silent: smoke worked once and then
    // never again for the rest of the game.
    LevelData d = parse(kTee);
    d.fuelDrain = 0.f;
    d.fuel = 500.f;
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    TouchControls t = phoneLayout(TouchScheme::Pad);
    InputManager in;
    const Vec2 btn = centreOf(t.rect(Action::Smoke));

    int bursts = 0;
    float last = r.fuel().fuel();
    for (int p = 0; p < 20; ++p) {
        t.fingerDown(1, btn.x, btn.y);
        for (int i = 0; i < 30; ++i) {
            in.beginFrame(); t.applyTo(in); r.update(in, s, static_cast<float>(FIXED_DT));
        }
        t.fingerUp(1);
        for (int i = 0; i < 10; ++i) {
            in.beginFrame(); t.applyTo(in); r.update(in, s, static_cast<float>(FIXED_DT));
        }
        if (last - r.fuel().fuel() > 0.1f) ++bursts;
        last = r.fuel().fuel();
    }
    CHECK_EQ(bursts, 20);       // every single press produced smoke
}
