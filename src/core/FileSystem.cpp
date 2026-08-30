#include "core/FileSystem.h"
#include <SDL.h>
#include <vector>

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

bool exists(const std::string& path) {
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) return false;
    SDL_RWclose(rw);
    return true;
}

} // namespace FileSystem
} // namespace rx
