package com.skiesofarcadia.standalone;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Process;

// Starts the game again for SkiesActivity.restartGame: runs in its own process (":restart"), ends the game's
// process, then launches SkiesActivity into a new one and exits.
public class SkiesRestartActivity extends Activity {
  static final String EXTRA_PID = "pid";

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    final int pid = getIntent().getIntExtra(EXTRA_PID, -1);
    if (pid > 0 && pid != Process.myPid()) {
      Process.killProcess(pid);
    }
    final Intent game = new Intent(this, SkiesActivity.class);
    game.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
    startActivity(game);
    finish();
    Runtime.getRuntime().exit(0);
  }
}
