#include "data/ScoreStore.h"
#include "core/FileSystem.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

namespace rx {
namespace {

// The characters a name may contain.  It is exactly the set the bitmap font
// can draw, so a stored name can never come back as a row of blanks.
bool nameChar(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

} // namespace

std::string ScoreStore::sanitizeName(const std::string& name) {
    std::string out;
    for (char c : name) {
        const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        // Anything else -- punctuation, accents, control characters, the tabs
        // and newlines that would break the file format -- is simply dropped.
        if (nameChar(up) && out.size() < ScoreRules::MAX_NAME_LENGTH) out.push_back(up);
    }
    return out.empty() ? std::string(ScoreRules::DEFAULT_NAME) : out;
}

void ScoreStore::setPlayerName(const std::string& name) {
    playerName_ = sanitizeName(name);
}

bool ScoreStore::qualifies(int score) const {
    if (score <= 0) return false;
    if (static_cast<int>(scores_.size()) < ScoreRules::HIGH_SCORE_LIMIT) return true;
    return score > scores_.back().score;
}

void ScoreStore::clear() {
    scores_.clear();
    runs_.clear();
    nextId_ = 1;
}

void ScoreStore::sortAndTrim() {
    // Highest first; a tie goes to whoever got there first, so beating a score
    // takes beating it, not matching it.
    std::stable_sort(scores_.begin(), scores_.end(),
                     [](const HighScoreRecord& a, const HighScoreRecord& b) {
                         if (a.score != b.score) return a.score > b.score;
                         return a.createdAt < b.createdAt;
                     });
    if (static_cast<int>(scores_.size()) > ScoreRules::HIGH_SCORE_LIMIT)
        scores_.resize(ScoreRules::HIGH_SCORE_LIMIT);

    // History is newest first and trimmed separately -- losing the oldest run
    // must never cost somebody their place in the table above.
    std::stable_sort(runs_.begin(), runs_.end(),
                     [](const RunRecord& a, const RunRecord& b) { return a.endedAt > b.endedAt; });
    if (static_cast<int>(runs_.size()) > ScoreRules::RUN_HISTORY_LIMIT)
        runs_.resize(ScoreRules::RUN_HISTORY_LIMIT);
}

int ScoreStore::recordRun(const RunRecord& run) {
    RunRecord r = run;
    r.playerName = sanitizeName(r.playerName);
    if (r.finalScore < 0) r.finalScore = 0;

    // Guard against the same finished run being filed twice -- a double call
    // on the game-over frame, a repeated event.  Identical runs a moment apart
    // are the bug, not the feature.
    if (!runs_.empty()) {
        const RunRecord& last = runs_.front();
        if (last.playerName == r.playerName && last.finalScore == r.finalScore &&
            last.levelReached == r.levelReached && last.endedAt == r.endedAt)
            return 0;
    }

    r.id = nextId_++;
    runs_.insert(runs_.begin(), r);

    int rank = 0;
    if (qualifies(r.finalScore)) {
        HighScoreRecord h;
        h.id         = nextId_++;
        h.playerName = r.playerName;
        h.score      = r.finalScore;
        h.level      = r.levelReached;
        h.createdAt  = r.endedAt;
        scores_.push_back(h);
    }

    sortAndTrim();

    for (size_t i = 0; i < scores_.size(); ++i)
        if (scores_[i].score == r.finalScore && scores_[i].createdAt == r.endedAt &&
            scores_[i].playerName == r.playerName) { rank = static_cast<int>(i) + 1; break; }

    save();     // written now, not at shutdown: a lost process must not cost a score
    return rank;
}

bool ScoreStore::renameLatest(const std::string& name) {
    if (runs_.empty()) return false;
    const std::string clean = sanitizeName(name);

    RunRecord& run = runs_.front();
    const std::string was = run.playerName;

    for (auto& h : scores_)
        if (h.playerName == was && h.score == run.finalScore && h.createdAt == run.endedAt)
            h.playerName = clean;

    run.playerName = clean;
    playerName_    = clean;      // and it becomes the name the next run uses
    return save();
}

// ---------------------------------------------------------------------------
// Storage
// ---------------------------------------------------------------------------

std::string ScoreStore::serialize() const {
    std::ostringstream out;
    out << "# Dakar X scores -- written by the game.  Safe to delete.\n";
    out << "version 1\n";
    out << "player " << playerName_ << "\n";
    // The name goes last on every line because it is the only field that could
    // ever contain a space; everything before it is a plain number.
    for (const auto& h : scores_)
        out << "score " << h.id << ' ' << h.score << ' ' << h.level << ' '
            << h.createdAt << ' ' << h.playerName << "\n";
    for (const auto& r : runs_)
        out << "run " << r.id << ' ' << r.finalScore << ' ' << r.levelReached << ' '
            << r.livesRemaining << ' ' << r.startedAt << ' ' << r.endedAt << ' '
            << r.playerName << "\n";
    return out.str();
}

void ScoreStore::parse(const std::string& text) {
    clear();
    std::istringstream in(text);
    std::string line;

    while (std::getline(in, line)) {
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        std::istringstream ls(t);
        std::string key;
        ls >> key;

        if (key == "player") {
            std::string rest;
            std::getline(ls, rest);
            playerName_ = sanitizeName(trim(rest));
        } else if (key == "score") {
            HighScoreRecord h;
            std::string name;
            if (!(ls >> h.id >> h.score >> h.level >> h.createdAt)) continue;
            std::getline(ls, name);
            h.playerName = sanitizeName(trim(name));
            scores_.push_back(h);
            nextId_ = std::max(nextId_, h.id + 1);
        } else if (key == "run") {
            RunRecord r;
            std::string name;
            if (!(ls >> r.id >> r.finalScore >> r.levelReached >> r.livesRemaining
                     >> r.startedAt >> r.endedAt)) continue;
            std::getline(ls, name);
            r.playerName = sanitizeName(trim(name));
            runs_.push_back(r);
            nextId_ = std::max(nextId_, r.id + 1);
        }
        // Anything else is from a newer version or a corrupted line: ignored,
        // never fatal.  A damaged file costs the damaged records and no more.
    }
    sortAndTrim();
}

bool ScoreStore::open(const std::string& path, const std::string& legacyPath) {
    path_     = path;
    ready_    = false;
    migrated_ = false;
    clear();

    std::string text;
    const bool haveOwn = FileSystem::readTextFile(path_, text);
    if (haveOwn) parse(text);

    // Look for a table written by a build from before the project was renamed
    // and adopt it wholesale -- scores, runs, player name and all.  The old
    // file is left exactly where it is: this copies, it does not move.
    //
    // The test is that this database is *empty*, not that its file is missing.
    // Those are not the same thing, and the difference is the whole migration:
    // the first launch after a rename creates an empty file, so keying on the
    // file's absence gives the migration exactly one chance and silently
    // forfeits it if that launch happened before the player restored a backup,
    // or with anything else amiss.  An empty table has nothing to lose, so
    // adopting into one is always safe and can be retried for as long as it
    // is needed.
    if (scores_.empty() && runs_.empty() &&
        !legacyPath.empty() && legacyPath != path_) {
        std::string legacy;
        if (FileSystem::readTextFile(legacyPath, legacy)) {
            parse(legacy);
            migrated_ = !scores_.empty() || !runs_.empty();
        }
    }

    if (haveOwn && !migrated_) {
        ready_ = true;      // nothing to write: what is on disk is current
        return true;
    }

    // Lay the database down, so the very first run has somewhere to go and so
    // a permissions problem shows up now rather than at the end of somebody's
    // best game.
    ready_ = save();
    return ready_;
}

bool ScoreStore::save() const {
    if (path_.empty()) return false;
    return FileSystem::writeFileAtomic(path_, serialize());
}

} // namespace rx
