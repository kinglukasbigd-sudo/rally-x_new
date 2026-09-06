#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rx {

// ---------------------------------------------------------------------------
// The local score database.
//
// It keeps two tables in one file: every finished run, and the best of those
// runs as a high-score table.  Both survive closing the game, because the file
// is written the moment a run ends rather than at shutdown -- an arcade
// cabinet losing power mid-game should not lose the score.
//
// The storage format is deliberately the same shape as the .lvl files: one
// record per line, a keyword first, human-readable throughout.  A player can
// open it, read it, and delete it, and a line the parser does not recognise is
// skipped rather than fatal.
// ---------------------------------------------------------------------------

// Everything worth retuning, in one place.
namespace ScoreRules {
// How many entries the high-score table holds.
constexpr int    HIGH_SCORE_LIMIT  = 10;
// How many finished runs are kept as history.  The high-score table is not
// trimmed with it: a good run stays in the table long after it has scrolled
// off the end of the history.
constexpr int    RUN_HISTORY_LIMIT = 50;
// Names are drawn with the game's 5x7 font in a 208px-wide viewport, so they
// are short by necessity as much as by tradition.
constexpr size_t MAX_NAME_LENGTH   = 8;
constexpr const char* DEFAULT_NAME = "PLAYER";
constexpr const char* FILE_NAME    = "scores.dat";
} // namespace ScoreRules

// One entry in the high-score table.
struct HighScoreRecord {
    uint64_t    id        = 0;
    std::string playerName;
    int         score     = 0;
    int         level     = 0;    // the round the run reached
    int64_t     createdAt = 0;    // unix seconds
};

// One complete play session, from the first round to the game-over screen.
struct RunRecord {
    uint64_t    id             = 0;
    std::string playerName;
    int         finalScore     = 0;
    int         levelReached   = 0;
    int         livesRemaining = 0;
    int64_t     startedAt      = 0;   // unix seconds
    int64_t     endedAt        = 0;

    int durationSeconds() const {
        return (endedAt > startedAt) ? static_cast<int>(endedAt - startedAt) : 0;
    }
};

class ScoreStore {
public:
    // Opens the file, creating it if it is not there.  Never throws and never
    // fails hard: a database that cannot be read leaves an empty table and the
    // game starts anyway, which is the whole point of keeping it out of the
    // gameplay path.
    //
    // `legacyPath`, when given, is a database written by an older build under
    // the project's previous name.  It is read only when there is nothing at
    // `path` yet, and it is never modified or removed -- a rename must not
    // cost anybody their scores, and it must not destroy the old copy either
    // in case the migration itself goes wrong.
    bool open(const std::string& path, const std::string& legacyPath = "");

    // True when this session's table was carried over from the old location.
    bool migrated() const { return migrated_; }

    // True when the file was read or created successfully.  False means the
    // game is running with scores it will not be able to keep.
    bool ready() const { return ready_; }
    const std::string& path() const { return path_; }

    // The name new runs are filed under.  Persisted with the scores, so the
    // player types it once.
    const std::string& playerName() const { return playerName_; }
    void setPlayerName(const std::string& name);

    // Trims a name to something the font can draw and the table can hold.
    static std::string sanitizeName(const std::string& name);

    // Would this score earn a place in the table as it stands?
    bool qualifies(int score) const;

    // Files a finished run: it always joins the run history, and joins the
    // high-score table if it earns a place.  Returns the 1-based rank it took
    // there, or 0 if it did not make the cut.  Writes the file immediately.
    int recordRun(const RunRecord& run);

    const std::vector<HighScoreRecord>& highScores() const { return scores_; }
    const std::vector<RunRecord>&       runs()       const { return runs_; }

    // The top of the table, or 0 when there is nothing in it yet.
    int bestScore() const { return scores_.empty() ? 0 : scores_.front().score; }

    // Re-files the most recent run, and its high-score entry if it earned one,
    // under a different name.  This is what the name-entry screen calls: the
    // run is saved the instant it ends, under whatever name was current, and
    // renamed afterwards -- so walking away mid-typing costs the name, never
    // the score.
    bool renameLatest(const std::string& name);

    // Writes the file.  Atomic: a full copy is written alongside and renamed
    // over the original, so a crash mid-write cannot leave a half-file.
    bool save() const;

    // Drops everything held in memory without touching the file.
    void clear();

    // Parsing and formatting, exposed so they can be tested without a disk.
    std::string serialize() const;
    void        parse(const std::string& text);

private:
    void sortAndTrim();

    std::string                  path_;
    std::string                  playerName_ = ScoreRules::DEFAULT_NAME;
    std::vector<HighScoreRecord> scores_;
    std::vector<RunRecord>       runs_;
    uint64_t                     nextId_  = 1;
    bool                         ready_   = false;
    bool                         migrated_ = false;
};

} // namespace rx
