#include "TestFramework.h"
#include "data/ScoreStore.h"
#include "core/FileSystem.h"
#include "core/NameEntry.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <string>

using namespace rx;

namespace {

// Each test gets its own file so the order they run in cannot matter.
std::string scratch(const char* name) {
    return std::string("build/test-db-") + name + ".dat";
}

struct Scratch {
    std::string path;
    explicit Scratch(const char* name) : path(scratch(name)) { std::remove(path.c_str()); }
    ~Scratch() { std::remove(path.c_str()); std::remove((path + ".tmp").c_str()); }
};

RunRecord makeRun(const char* name, int score, int level, int64_t ended) {
    RunRecord r;
    r.playerName     = name;
    r.finalScore     = score;
    r.levelReached   = level;
    r.livesRemaining = 0;
    r.startedAt      = ended - 300;
    r.endedAt        = ended;
    return r;
}

} // namespace

// --- the file --------------------------------------------------------------

TEST(a_missing_database_is_created_rather_than_fatal) {
    Scratch s("create");
    CHECK(!FileSystem::exists(s.path));

    ScoreStore db;
    CHECK(db.open(s.path));
    CHECK(db.ready());
    CHECK(db.highScores().empty());
    CHECK(db.runs().empty());
    CHECK_EQ(db.bestScore(), 0);
    // Laid down straight away, so a write problem shows up before somebody's
    // best game rather than after it.
    CHECK(FileSystem::exists(s.path));
}

TEST(an_existing_database_is_opened_and_never_overwritten) {
    Scratch s("reopen");
    {
        ScoreStore db;
        CHECK(db.open(s.path));
        db.recordRun(makeRun("IVAN", 25000, 7, 1000));
    }
    {
        ScoreStore db;
        CHECK(db.open(s.path));
        CHECK_EQ(static_cast<int>(db.highScores().size()), 1);
        CHECK_STR(db.highScores()[0].playerName, "IVAN");
        CHECK_EQ(db.highScores()[0].score, 25000);
        CHECK_EQ(db.highScores()[0].level, 7);
        CHECK_EQ(db.bestScore(), 25000);
    }
}

TEST(a_score_survives_closing_and_reopening_the_application) {
    // The scenario from the brief, walked through step by step.
    Scratch s("restart");

    {   // 1-4: a session runs, Ivan scores 25,000, the run ends and is saved.
        ScoreStore db;
        CHECK(db.open(s.path));
        db.setPlayerName("IVAN");
        CHECK_EQ(db.recordRun(makeRun("IVAN", 25000, 9, 5000)), 1);
    }
    {   // 5-8: the application is gone and comes back.  The score is still there.
        ScoreStore db;
        CHECK(db.open(s.path));
        CHECK_EQ(static_cast<int>(db.highScores().size()), 1);
        CHECK_STR(db.highScores()[0].playerName, "IVAN");
        CHECK_EQ(db.highScores()[0].score, 25000);
        CHECK_STR(db.playerName(), "IVAN");   // and so is the name

        // A better run, then another restart.
        CHECK_EQ(db.recordRun(makeRun("IVAN", 41000, 12, 6000)), 1);
    }
    {
        ScoreStore db;
        CHECK(db.open(s.path));
        CHECK_EQ(static_cast<int>(db.highScores().size()), 2);
        CHECK_EQ(db.highScores()[0].score, 41000);        // the new one on top
        CHECK_EQ(db.highScores()[1].score, 25000);        // the old one kept
        CHECK_EQ(db.bestScore(), 41000);
    }
}

TEST(a_new_run_never_erases_what_is_already_there) {
    Scratch s("keep");
    ScoreStore db;
    CHECK(db.open(s.path));
    db.recordRun(makeRun("IVAN", 30000, 8, 100));

    // A dreadful run afterwards must not cost the good one its place.
    db.recordRun(makeRun("ALEX", 300, 1, 200));
    CHECK_EQ(static_cast<int>(db.highScores().size()), 2);
    CHECK_STR(db.highScores()[0].playerName, "IVAN");
    CHECK_EQ(db.highScores()[0].score, 30000);
}

TEST(a_corrupt_file_costs_the_damaged_lines_and_nothing_else) {
    Scratch s("corrupt");
    const std::string junk =
        "version 1\n"
        "player IVAN\n"
        "score 1 25000 7 900 IVAN\n"
        "score this line is nonsense\n"
        "\x01\x02 binary rubbish\n"
        "run 2 25000 7 0 600 900 IVAN\n"
        "unknown-keyword 1 2 3\n";
    CHECK(FileSystem::writeFileAtomic(s.path, junk));

    ScoreStore db;
    CHECK(db.open(s.path));                       // opens rather than crashing
    CHECK_EQ(static_cast<int>(db.highScores().size()), 1);
    CHECK_EQ(static_cast<int>(db.runs().size()), 1);
    CHECK_EQ(db.highScores()[0].score, 25000);
    CHECK_STR(db.playerName(), "IVAN");
}

TEST(an_unwritable_path_leaves_the_game_playable) {
    ScoreStore db;
    // A directory that does not exist and cannot be created.
    CHECK(!db.open("/proc/definitely/not/writable/scores.dat"));
    CHECK(!db.ready());
    // Still perfectly usable in memory -- the run is simply not kept.
    CHECK_EQ(db.recordRun(makeRun("IVAN", 1000, 2, 10)), 1);
    CHECK_EQ(static_cast<int>(db.highScores().size()), 1);
}

TEST(the_write_is_atomic_and_leaves_no_temporary_behind) {
    Scratch s("atomic");
    ScoreStore db;
    CHECK(db.open(s.path));
    db.recordRun(makeRun("IVAN", 1234, 3, 77));
    CHECK(FileSystem::exists(s.path));
    CHECK(!FileSystem::exists(s.path + ".tmp"));
}

// --- the table -------------------------------------------------------------

TEST(the_table_is_sorted_highest_first) {
    Scratch s("sort");
    ScoreStore db;
    CHECK(db.open(s.path));
    db.recordRun(makeRun("MARK", 72500,  8, 100));
    db.recordRun(makeRun("IVAN", 128450, 14, 200));
    db.recordRun(makeRun("ALEX", 91200,  10, 300));

    const auto& t = db.highScores();
    CHECK_EQ(static_cast<int>(t.size()), 3);
    CHECK_STR(t[0].playerName, "IVAN"); CHECK_EQ(t[0].score, 128450); CHECK_EQ(t[0].level, 14);
    CHECK_STR(t[1].playerName, "ALEX"); CHECK_EQ(t[1].score, 91200);  CHECK_EQ(t[1].level, 10);
    CHECK_STR(t[2].playerName, "MARK"); CHECK_EQ(t[2].score, 72500);  CHECK_EQ(t[2].level, 8);
}

TEST(a_tie_goes_to_whoever_got_there_first) {
    Scratch s("tie");
    ScoreStore db;
    CHECK(db.open(s.path));
    db.recordRun(makeRun("FIRST",  5000, 3, 100));
    db.recordRun(makeRun("SECOND", 5000, 3, 200));
    CHECK_STR(db.highScores()[0].playerName, "FIRST");
}

TEST(the_table_holds_the_best_n_and_drops_the_rest) {
    Scratch s("topn");
    ScoreStore db;
    CHECK(db.open(s.path));

    // Twice the limit, worst first, so every one of them qualifies on the way in.
    const int n = ScoreRules::HIGH_SCORE_LIMIT;
    for (int i = 1; i <= n * 2; ++i)
        db.recordRun(makeRun("P", i * 1000, i, 1000 + i));

    CHECK_EQ(static_cast<int>(db.highScores().size()), n);
    CHECK_EQ(db.highScores().front().score, n * 2 * 1000);
    CHECK_EQ(db.highScores().back().score,  (n + 1) * 1000);

    // Anything below the tail no longer qualifies...
    CHECK(!db.qualifies(1000));
    CHECK(!db.qualifies(db.highScores().back().score));   // matching is not beating
    CHECK(db.qualifies(db.highScores().back().score + 1));

    // ...and does not displace anybody.
    CHECK_EQ(db.recordRun(makeRun("NOBODY", 500, 1, 9999)), 0);
    CHECK_EQ(static_cast<int>(db.highScores().size()), n);
}

TEST(an_empty_table_takes_anything_worth_more_than_nothing) {
    Scratch s("empty");
    ScoreStore db;
    CHECK(db.open(s.path));
    CHECK(!db.qualifies(0));        // a run that scored nothing is not a score
    CHECK(db.qualifies(1));
    CHECK_EQ(db.recordRun(makeRun("IVAN", 0, 1, 5)), 0);
    CHECK(db.highScores().empty());
    CHECK_EQ(static_cast<int>(db.runs().size()), 1);   // but it is still a run
}

// --- runs ------------------------------------------------------------------

TEST(a_run_keeps_its_name_score_level_lives_and_times) {
    Scratch s("runrec");
    ScoreStore db;
    CHECK(db.open(s.path));

    RunRecord r = makeRun("IVAN", 12345, 6, 2000);
    r.livesRemaining = 2;
    r.startedAt = 1700;
    db.recordRun(r);

    {
        ScoreStore reopened;
        CHECK(reopened.open(s.path));
        CHECK_EQ(static_cast<int>(reopened.runs().size()), 1);
        const RunRecord& got = reopened.runs()[0];
        CHECK_STR(got.playerName, "IVAN");
        CHECK_EQ(got.finalScore, 12345);
        CHECK_EQ(got.levelReached, 6);
        CHECK_EQ(got.livesRemaining, 2);
        CHECK_EQ(static_cast<int>(got.startedAt), 1700);
        CHECK_EQ(static_cast<int>(got.endedAt), 2000);
        CHECK_EQ(got.durationSeconds(), 300);
        CHECK(got.id > 0);
    }
}

TEST(every_record_gets_its_own_id) {
    Scratch s("ids");
    ScoreStore db;
    CHECK(db.open(s.path));
    for (int i = 1; i <= 5; ++i) db.recordRun(makeRun("P", i * 100, i, 500 + i));

    std::vector<uint64_t> ids;
    for (const auto& r : db.runs())       ids.push_back(r.id);
    for (const auto& h : db.highScores()) ids.push_back(h.id);
    std::sort(ids.begin(), ids.end());
    CHECK(std::adjacent_find(ids.begin(), ids.end()) == ids.end());
    CHECK(ids.front() > 0);

    // Ids keep climbing across a restart rather than starting over and
    // colliding with what is already filed.
    ScoreStore reopened;
    CHECK(reopened.open(s.path));
    reopened.recordRun(makeRun("P", 99999, 9, 9000));
    CHECK(reopened.runs().front().id > ids.back());
}

TEST(the_run_history_is_trimmed_without_touching_the_table) {
    Scratch s("history");
    ScoreStore db;
    CHECK(db.open(s.path));

    // One very good run, then enough dull ones to push it out of the history.
    db.recordRun(makeRun("IVAN", 500000, 40, 1));
    for (int i = 0; i < ScoreRules::RUN_HISTORY_LIMIT + 5; ++i)
        db.recordRun(makeRun("FILLER", 10, 1, 100 + i));

    CHECK_EQ(static_cast<int>(db.runs().size()), ScoreRules::RUN_HISTORY_LIMIT);
    // Scrolled off the history, still top of the table.  This is the whole
    // reason the two are kept separately.
    CHECK_STR(db.highScores()[0].playerName, "IVAN");
    CHECK_EQ(db.highScores()[0].score, 500000);
}

TEST(the_same_finished_run_filed_twice_is_only_counted_once) {
    Scratch s("dupe");
    ScoreStore db;
    CHECK(db.open(s.path));

    const RunRecord r = makeRun("IVAN", 7000, 4, 4242);
    CHECK_EQ(db.recordRun(r), 1);
    CHECK_EQ(db.recordRun(r), 0);          // the double call is ignored
    CHECK_EQ(static_cast<int>(db.runs().size()), 1);
    CHECK_EQ(static_cast<int>(db.highScores().size()), 1);

    // A genuinely different run at the same instant is still recorded.
    CHECK_EQ(db.recordRun(makeRun("IVAN", 7001, 4, 4242)), 1);
    CHECK_EQ(static_cast<int>(db.runs().size()), 2);
}

// --- names -----------------------------------------------------------------

TEST(a_name_is_trimmed_to_something_the_game_can_draw) {
    CHECK_STR(ScoreStore::sanitizeName("Ivan"), "IVAN");
    CHECK_STR(ScoreStore::sanitizeName("ivan"), "IVAN");
    // Long names are cut rather than refused.
    CHECK_STR(ScoreStore::sanitizeName("ABCDEFGHIJKL"), "ABCDEFGH");
    // Anything the font cannot draw is dropped, tabs and newlines included --
    // they would otherwise break the one-record-per-line file format.
    CHECK_STR(ScoreStore::sanitizeName("I v\tan\n"), "IVAN");
    CHECK_STR(ScoreStore::sanitizeName("A-1.B"), "A-1.B");
    // Nothing usable left over falls back rather than filing a blank.
    CHECK_STR(ScoreStore::sanitizeName(""), ScoreRules::DEFAULT_NAME);
    CHECK_STR(ScoreStore::sanitizeName("!!!"), ScoreRules::DEFAULT_NAME);
}

TEST(a_name_with_control_characters_cannot_corrupt_the_file) {
    Scratch s("evil");
    ScoreStore db;
    CHECK(db.open(s.path));
    // A name that, stored raw, would look like two more records.
    db.recordRun(makeRun("A\nscore 9 999999 99 1 CHEAT", 100, 1, 50));

    ScoreStore reopened;
    CHECK(reopened.open(s.path));
    CHECK_EQ(static_cast<int>(reopened.highScores().size()), 1);
    CHECK_EQ(reopened.highScores()[0].score, 100);          // no injected record
}

TEST(the_player_name_is_remembered_between_sessions) {
    Scratch s("name");
    {
        ScoreStore db;
        CHECK(db.open(s.path));
        CHECK_STR(db.playerName(), ScoreRules::DEFAULT_NAME);
        db.setPlayerName("Alex");
        CHECK(db.save());
    }
    {
        ScoreStore db;
        CHECK(db.open(s.path));
        CHECK_STR(db.playerName(), "ALEX");
    }
}

TEST(renaming_the_latest_run_renames_its_table_entry_too) {
    Scratch s("rename");
    ScoreStore db;
    CHECK(db.open(s.path));
    db.recordRun(makeRun("PLAYER", 8000, 5, 300));
    CHECK(db.renameLatest("Ivan"));

    CHECK_STR(db.runs()[0].playerName, "IVAN");
    CHECK_STR(db.highScores()[0].playerName, "IVAN");
    CHECK_STR(db.playerName(), "IVAN");

    ScoreStore reopened;
    CHECK(reopened.open(s.path));
    CHECK_STR(reopened.highScores()[0].playerName, "IVAN");
}

TEST(several_players_share_one_table) {
    Scratch s("multi");
    ScoreStore db;
    CHECK(db.open(s.path));
    db.setPlayerName("IVAN"); db.recordRun(makeRun("IVAN", 128450, 14, 100));
    db.setPlayerName("ALEX"); db.recordRun(makeRun("ALEX",  91200, 10, 200));
    db.setPlayerName("MARK"); db.recordRun(makeRun("MARK",  72500,  8, 300));
    db.setPlayerName("IVAN"); db.recordRun(makeRun("IVAN",  60000,  7, 400));

    ScoreStore reopened;
    CHECK(reopened.open(s.path));
    const auto& t = reopened.highScores();
    CHECK_EQ(static_cast<int>(t.size()), 4);
    CHECK_STR(t[0].playerName, "IVAN");
    CHECK_STR(t[1].playerName, "ALEX");
    CHECK_STR(t[2].playerName, "MARK");
    CHECK_STR(t[3].playerName, "IVAN");   // two entries, one player
    CHECK_STR(reopened.playerName(), "IVAN");
}

// --- name entry ------------------------------------------------------------

TEST(the_name_grid_holds_every_character_a_name_may_contain) {
    const std::string set(NameEntry::charset(), NameEntry::COLS * NameEntry::ROWS);
    CHECK_EQ(static_cast<int>(set.size()), 40);
    for (char c = 'A'; c <= 'Z'; ++c) CHECK(set.find(c) != std::string::npos);
    for (char c = '0'; c <= '9'; ++c) CHECK(set.find(c) != std::string::npos);
    CHECK(set.find('<') != std::string::npos);   // rub out
    CHECK(set.find('>') != std::string::npos);   // end
}

TEST(the_grid_spells_out_a_name) {
    NameEntry e;
    e.begin("");
    const std::string set(NameEntry::charset(), NameEntry::COLS * NameEntry::ROWS);

    for (char want : std::string("IVAN")) {
        e.setCursor(static_cast<int>(set.find(want)));
        e.commit();
    }
    CHECK_STR(e.text(), "IVAN");
    CHECK(!e.done());

    e.setCursor(static_cast<int>(set.find('<')));
    e.commit();
    CHECK_STR(e.text(), "IVA");

    e.setCursor(static_cast<int>(set.find('>')));
    e.commit();
    CHECK(e.done());
    CHECK_STR(e.result(), "IVA");
}

TEST(the_cursor_wraps_in_both_directions) {
    NameEntry e;
    e.begin("");
    CHECK_EQ(e.cursor(), 0);
    e.move(Direction::Left);
    CHECK_EQ(e.cursor(), NameEntry::COLS - 1);        // wrapped along the row
    e.move(Direction::Right);
    CHECK_EQ(e.cursor(), 0);
    e.move(Direction::Up);
    CHECK_EQ(e.cursor(), NameEntry::COLS * (NameEntry::ROWS - 1));   // and the column
    e.move(Direction::Down);
    CHECK_EQ(e.cursor(), 0);
}

TEST(a_typed_name_is_capped_and_filtered) {
    NameEntry e;
    e.begin("");
    for (char c : std::string("ivan-the-terrible")) e.typeChar(c);
    CHECK_EQ(static_cast<int>(e.text().size()),
             static_cast<int>(ScoreRules::MAX_NAME_LENGTH));
    CHECK_STR(e.text(), "IVAN-THE");

    e.backspace();
    CHECK_STR(e.text(), "IVAN-TH");
    // A character the grid does not have is simply not typed.
    e.typeChar('%');
    CHECK_STR(e.text(), "IVAN-TH");
}

TEST(entry_starts_from_the_name_already_on_file) {
    NameEntry e;
    e.begin("ALEX");
    CHECK_STR(e.text(), "ALEX");

    // ...except the placeholder, which nobody chose and should not have to
    // rub out one character at a time.
    e.begin(ScoreRules::DEFAULT_NAME);
    CHECK(e.text().empty());
    // Confirming an empty field still files something legible.
    CHECK_STR(e.result(), ScoreRules::DEFAULT_NAME);
}

// --- carrying the table across the rename ---------------------------------

TEST(a_table_written_under_the_old_name_is_adopted_not_lost) {
    // The migration the project rename needed: everything a player had before
    // has to still be there afterwards.
    Scratch legacy("legacy-src");
    Scratch fresh("legacy-dst");

    {   // What the old build left behind.
        ScoreStore old;
        CHECK(old.open(legacy.path));
        old.setPlayerName("IVAN");
        old.recordRun(makeRun("IVAN", 128450, 14, 100));
        old.recordRun(makeRun("ALEX",  91200, 10, 200));
    }

    // The renamed build starting for the first time: nothing of its own yet.
    ScoreStore db;
    CHECK(db.open(fresh.path, legacy.path));
    CHECK(db.migrated());
    CHECK_EQ(static_cast<int>(db.highScores().size()), 2);
    CHECK_EQ(db.highScores()[0].score, 128450);
    CHECK_STR(db.highScores()[0].playerName, "IVAN");
    CHECK_EQ(static_cast<int>(db.runs().size()), 2);
    CHECK_STR(db.playerName(), "IVAN");          // and the name carries over

    // Written to the new location straight away, so the next launch needs no
    // migration at all...
    ScoreStore again;
    CHECK(again.open(fresh.path, legacy.path));
    CHECK(!again.migrated());
    CHECK_EQ(static_cast<int>(again.highScores().size()), 2);

    // ...and the old file is untouched, in case anything went wrong.
    ScoreStore old;
    CHECK(old.open(legacy.path));
    CHECK_EQ(static_cast<int>(old.highScores().size()), 2);
}

TEST(an_existing_table_is_never_replaced_by_an_older_one) {
    Scratch legacy("nostomp-src");
    Scratch fresh("nostomp-dst");
    {
        ScoreStore old;
        CHECK(old.open(legacy.path));
        old.recordRun(makeRun("OLD", 999999, 40, 100));
    }
    {   // The new location already has a table of its own.
        ScoreStore db;
        CHECK(db.open(fresh.path));
        db.recordRun(makeRun("NEW", 500, 2, 200));
    }

    ScoreStore db;
    CHECK(db.open(fresh.path, legacy.path));
    CHECK(!db.migrated());
    CHECK_EQ(static_cast<int>(db.highScores().size()), 1);
    CHECK_STR(db.highScores()[0].playerName, "NEW");   // not overwritten
}

TEST(a_missing_or_empty_old_location_is_simply_a_fresh_start) {
    Scratch fresh("nolegacy");
    ScoreStore db;
    CHECK(db.open(fresh.path, "build/there-is-no-such-file.dat"));
    CHECK(db.ready());
    CHECK(!db.migrated());
    CHECK(db.highScores().empty());
    CHECK_STR(db.playerName(), ScoreRules::DEFAULT_NAME);
}
