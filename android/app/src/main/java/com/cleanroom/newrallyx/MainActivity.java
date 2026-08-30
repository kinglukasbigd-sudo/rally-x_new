package com.cleanroom.newrallyx;

import org.libsdl.app.SDLActivity;

/**
 * New Rally-X on Android.
 *
 * Everything about the game lives in the shared C++ core; this class exists
 * only to tell SDL which native libraries to load. Input, rendering, audio and
 * asset loading all go through SDL, so there is no Android-specific gameplay
 * code anywhere in the project.
 */
public class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
