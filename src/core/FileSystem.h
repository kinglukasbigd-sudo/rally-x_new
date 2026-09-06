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

// The one directory the game may write to, with a trailing separator.  On
// desktop that is the platform's per-user preferences path; on Android it is
// the app's private internal storage, which is writable without asking for a
// permission and is cleaned up when the app is uninstalled.  Created if it is
// not there.  Returns "./" if even that fails, so a caller always has a path.
std::string writableDataDir();

// Where this game's data used to live, before the project was renamed.  Empty
// when there is no such place on this platform.  It exists so a player who had
// scores under the old name keeps them; nothing else should ever read it.
std::string legacyDataDir();

// Writes a file so that it either lands complete or does not land at all: the
// data goes to a neighbouring temporary file, is flushed to the disk, and is
// then renamed over the target in one step.  A crash or a power cut can lose
// the write but can never leave a half-written file behind.
bool writeFileAtomic(const std::string& path, const std::string& data);

} // namespace FileSystem
} // namespace rx
