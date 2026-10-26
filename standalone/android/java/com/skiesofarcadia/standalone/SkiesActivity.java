package com.skiesofarcadia.standalone;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

// The emulator-free host on Android: libmain.so is the standalone with SDL linked in (Aurora takes SDL's
// activity mutex, which a shared libSDL3.so doesn't export), so only it is loaded.
public class SkiesActivity extends SDLActivity {
  @Override
  protected String[] getLibraries() {
    return new String[] {"main"};
  }

  // The Language restart (main.cpp Restart): SkiesRestartActivity, in a process of its own, ends this one and
  // starts the game again (SDL runs the native main once per process). Called from the game's thread.
  public void restartGame() {
    final Intent intent = new Intent(this, SkiesRestartActivity.class);
    intent.putExtra(SkiesRestartActivity.EXTRA_PID, Process.myPid());
    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    startActivity(intent);
  }

  // Immersive from the start: the surface is the whole screen before Aurora sizes its swap chain (resizing it
  // later recreates the EFB and drops the copies that live only on the GPU). A swipe shows the bars for a while.
  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    hideSystemBars();
  }

  @Override
  public void onWindowFocusChanged(boolean hasFocus) {
    super.onWindowFocusChanged(hasFocus);
    if (hasFocus) {
      hideSystemBars();
    }
  }

  private void hideSystemBars() {
    final Window window = getWindow();
    window.getAttributes().layoutInDisplayCutoutMode =
        WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
      window.setDecorFitsSystemWindows(false);
      final WindowInsetsController controller = window.getInsetsController();
      if (controller != null) {
        controller.hide(WindowInsets.Type.systemBars());
        controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
      }
    } else {
      window.getDecorView().setSystemUiVisibility(
          View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
          View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
          View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }
  }
}
