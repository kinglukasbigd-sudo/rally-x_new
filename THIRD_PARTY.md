# Third-party components

## SDL2

The Android project vendors SDL2's Java glue under
`android/app/src/main/java/org/libsdl/app/`. These files are SDL2's own,
unmodified, and are used under the **zlib licence**:

> Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>
>
> This software is provided 'as-is', without any express or implied warranty.
> In no event will the authors be held liable for any damages arising from the
> use of this software.
>
> Permission is granted to anyone to use this software for any purpose,
> including commercial applications, and to alter it and redistribute it
> freely, subject to the following restrictions:
>
> 1. The origin of this software must not be misrepresented; you must not claim
>    that you wrote the original software. If you use this software in a
>    product, an acknowledgment in the product documentation would be
>    appreciated but is not required.
> 2. Altered source versions must be plainly marked as such, and must not be
>    misrepresented as being the original software.
> 3. This notice may not be removed or altered from any source distribution.

The SDL2 C sources are **not** committed here. Both builds fetch them:
`tools/fetch_sdl2.sh` stages the desktop headers, `tools/fetch_sdl2_android.sh`
fetches the full source tree the Android build compiles.

## Gradle wrapper

`android/gradle/wrapper/gradle-wrapper.jar` and the `gradlew` scripts are the
standard Gradle wrapper, distributed under the Apache License 2.0.

## Background music

`assets/audio/music_normal.wav` and `assets/audio/music_challenge.wav` are built
from tracks supplied by the project owner (see the README's *Background music*
section for the tooling). They are **not** covered by the clean-room statement
and are not connected to the original game — whatever rights attach to those
source tracks attach to these files.

## The original game

*New Rally-X* and *Rally-X* are trademarks of their respective owner. This
project is an independent clean-room reconstruction of the 1981 game's
behaviour; it contains no code, artwork, audio, or data from the original, and
is not affiliated with or endorsed by the trademark holder. See the README's
*Clean-room statement*.
