package com.skiesofarcadia.standalone;

import org.libsdl.app.SDLActivity;

// The emulator-free host on Android: libmain.so is the standalone with SDL linked in (Aurora takes SDL's
// activity mutex, which a shared libSDL3.so doesn't export), so only it is loaded.
public class SkiesActivity extends SDLActivity {
  @Override
  protected String[] getLibraries() {
    return new String[] {"main"};
  }
}
