package org.dolphinemu.dolphinemu.skies;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.util.Log;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.RelativeLayout;

import org.libsdl.app.SDLActivity;

/**
 * The game in the branded app: the emulator-free host (libskies.so, SDL and Aurora linked in) in a process of its
 * own (":game", AndroidManifest), so Dolphin's libmain.so (the launcher's imports and settings) never shares one with
 * it. SkiesLauncherActivity starts it through SkiesGame with the host's command line in EXTRA_ARGS.
 * <p>
 * SDL runs the native main once per process (a second start in the same process exits), so the process ends with the
 * activity. Closing: Quit game in the in-game menu, or the launcher's ACTION_CLOSE broadcast (before an import, a
 * new disc, ...): both let main return, so the memory card is flushed, then SDL finishes the activity. Back opens the
 * in-game menu, as in the Dolphin build.
 */
public class SkiesGameActivity extends SDLActivity {
  public static final String EXTRA_ARGS = "org.dolphinemu.dolphinemu.skies.GAME_ARGS";
  public static final String ACTION_CLOSE = "org.dolphinemu.dolphinemu.skies.CLOSE_GAME";
  /** The result for the launcher: the in-game menu's Language restart wants the game started again. */
  public static final int RESULT_RESTART = RESULT_FIRST_USER + 1;
  private static final String TAG = "SkiesGame";
  /** After a close request, how long main gets to return before the activity is finished anyway. */
  private static final long CLOSE_TIMEOUT_MS = 10000;

  /** The in-game menu (Back, and the touch controls' menu button). */
  static native void nativeBack();

  private final Handler mHandler = new Handler(Looper.getMainLooper());
  private boolean mClosing = false;
  private SkiesTouchControls mTouch;

  private final BroadcastReceiver mCloseReceiver = new BroadcastReceiver() {
    @Override
    public void onReceive(Context context, Intent intent) {
      requestClose();
    }
  };

  @Override
  protected String[] getLibraries() {
    return new String[] {"skies"};
  }

  @Override
  protected String[] getArguments() {
    final String[] args = getIntent().getStringArrayExtra(EXTRA_ARGS);
    return args != null ? args : new String[0];
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    hideSystemBars();
    // The touch controls over the game's surface (SDL's layout holds it; null when the libraries didn't load)
    if (mLayout != null && !SDLActivity.mBrokenLibraries) {
      mTouch = new SkiesTouchControls(this);
      mLayout.addView(mTouch, new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.MATCH_PARENT,
                                                              RelativeLayout.LayoutParams.MATCH_PARENT));
    }
    final IntentFilter filter = new IntentFilter(ACTION_CLOSE);
    if (Build.VERSION.SDK_INT >= 33) {
      registerReceiver(mCloseReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
    } else {
      registerReceiver(mCloseReceiver, filter);
    }
  }

  /** Lets main return (SDL's quit event: the host saves its card and shuts down), then SDL finishes this. */
  private void requestClose() {
    if (mClosing) {
      return;
    }
    mClosing = true;
    Log.i(TAG, "close requested");
    SDLActivity.nativeSendQuit();
    mHandler.postDelayed(() -> {
      if (!isFinishing()) {
        Log.w(TAG, "the game didn't end in time");
        finish();
      }
    }, CLOSE_TIMEOUT_MS);
  }

  // The Language restart (main.cpp Restart, on the game's thread just before main returns): the launcher starts
  // the game again once this process has ended.
  public void restartGame() {
    setResult(RESULT_RESTART);
  }

  @Override
  public void onBackPressed() {
    // The in-game menu, as Esc on the desktop (Quit game is in it); not SDL's default, which ends the game
    if (SDLActivity.mBrokenLibraries) {
      super.onBackPressed();
      return;
    }
    Log.i(TAG, "Back: the menu");
    nativeBack();
  }

  // A controller's buttons and sticks: the touch controls step aside until the screen is touched again
  private static boolean FromController(InputEvent event) {
    return (event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
           (event.getSource() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
  }

  @Override
  public boolean dispatchKeyEvent(KeyEvent event) {
    if (mTouch != null && FromController(event) && event.getKeyCode() != KeyEvent.KEYCODE_BACK)
      mTouch.onControllerInput();
    return super.dispatchKeyEvent(event);
  }

  @Override
  public boolean dispatchGenericMotionEvent(MotionEvent event) {
    if (mTouch != null && FromController(event))
      mTouch.onControllerInput();
    return super.dispatchGenericMotionEvent(event);
  }

  @Override
  protected void onDestroy() {
    if (mTouch != null)
      mTouch.release();
    try {
      unregisterReceiver(mCloseReceiver);
    } catch (IllegalArgumentException e) {
      // not registered (onCreate stopped early)
    }
    super.onDestroy();
    // SDL can't run main a second time in this process: end it with the activity
    if (isFinishing()) {
      Log.i(TAG, "game ended");
      Process.killProcess(Process.myPid());
    }
  }

  // Immersive from the start: the surface is the whole screen before Aurora sizes its swap chain (resizing it later
  // recreates the EFB and drops the copies that live only on the GPU). A swipe shows the bars for a while.
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
    window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
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
