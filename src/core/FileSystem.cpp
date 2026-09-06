#include "core/FileSystem.h"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace rx {
namespace FileSystem {

bool readTextFile(const std::string& path, std::string& out) {
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) return false;

    const Sint64 size = SDL_RWsize(rw);
    if (size < 0) { SDL_RWclose(rw); return false; }

    out.assign(static_cast<size_t>(size), '\0');
    size_t read = 0;
    while (read < out.size()) {
        const size_t n = SDL_RWread(rw, &out[read], 1, out.size() - read);
        if (n == 0) break;
        read += n;
    }
    SDL_RWclose(rw);
    out.resize(read);
    return read > 0;
}

std::string writableDataDir() {
#if defined(__ANDROID__)
    // Private to the app: no storage permission needed, and it survives every
    // update.  This is where the score database lives on a phone.
    if (const char* p = SDL_AndroidGetInternalStoragePath())
        return std::string(p) + "/";
#endif
    // SDL creates the directory as a side effect, which is exactly what is
    // wanted on a first run.
    if (char* p = SDL_GetPrefPath("cleanroom", "dakarx")) {
        std::string dir(p);
        SDL_free(p);
        if (!dir.empty()) return dir;
    }
    return "./";
}

std::string legacyDataDir() {
#if defined(__ANDROID__)
    // Nothing to return.  The old build was a different package, and Android
    // walls one app's private storage off from another's, so the old data is
    // not reachable from here however much we would like it to be.
    return "";
#else
    // Assembled by hand rather than asked of SDL_GetPrefPath, because that
    // creates whatever it is asked for and would leave an empty directory
    // behind for every player who never ran the old build.  The shape has to
    // match what SDL_GetPrefPath itself produced back then, organisation
    // segment included -- get that wrong and the migration quietly finds
    // nothing, which looks exactly like having had nothing to migrate.
    if (const char* xdg = std::getenv("XDG_DATA_HOME"))
        return std::string(xdg) + "/cleanroom/newrallyx/";
    if (const char* home = std::getenv("HOME"))
        return std::string(home) + "/.local/share/cleanroom/newrallyx/";
    return "";
#endif
}

bool writeFileAtomic(const std::string& path, const std::string& data) {
    const std::string tmp = path + ".tmp";

    std::FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;

    bool ok = data.empty() ||
              std::fwrite(data.data(), 1, data.size(), f) == data.size();
    if (ok) ok = (std::fflush(f) == 0);
#if !defined(_WIN32)
    // Without this the rename can reach the disk before the contents do, and a
    // power cut leaves an empty file where the scores used to be.
    if (ok) ok = (::fsync(fileno(f)) == 0);
#endif
    if (std::fclose(f) != 0) ok = false;

    if (!ok) { std::remove(tmp.c_str()); return false; }

#if defined(_WIN32)
    std::remove(path.c_str());        // rename will not overwrite on Windows
#endif
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

bool exists(const std::string& path) {
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) return false;
    SDL_RWclose(rw);
    return true;
}

} // namespace FileSystem
} // namespace rx
