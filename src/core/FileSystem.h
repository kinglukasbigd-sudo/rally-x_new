#pragma once
#include <string>

namespace rx {

// All data files go through here.  On desktop this is an ordinary file read;
// on Android the same call reads straight out of the APK's asset bundle,
// because SDL's RWops layer resolves relative paths to assets there.  Nothing
// else in the game needs to know the difference.
namespace FileSystem {

bool readTextFile(const std::string& path, std::string& out);
bool exists(const std::string& path);

} // namespace FileSystem
} // namespace rx
