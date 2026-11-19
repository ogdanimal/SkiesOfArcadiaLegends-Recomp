package org.dolphinemu.dolphinemu.skies;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.hardware.input.InputManager;
import android.os.Handler;
import android.os.Looper;
import android.view.HapticFeedbackConstants;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

/**
 * On-screen GameCube controls for the emulator-free host, for phones without a controller: the stick, the C-stick,
 * the D-pad, A / B / X / Y, L / R (fully in, with the click), Z, Start and a menu button (the in-game menu, as Back).
 * They go to controller 1 beside a real one (video.cpp VideoPadRead: buttons added, the stick and C-stick used while
 * touched, the triggers the further in of the two).
 * <p>
 * Shown while no controller is connected; a controller's input hides them and a touch on the screen shows them
 * again, so a device with built-in controls (the Retroid) starts without them. Laid out for landscape, sized by
 * the screen's shorter side.
 */
public class SkiesTouchControls extends View implements InputManager.InputDeviceListener {
  // controller 1's button bits (the GameCube's PAD_BUTTON_* / PAD_TRIGGER_*)
  static final int LEFT = 0x1, RIGHT = 0x2, DOWN = 0x4, UP = 0x8, Z = 0x10, R = 0x20, L = 0x40, A = 0x100,
      B = 0x200, X = 0x400, Y = 0x800, START = 0x1000;

  /** buttons: the bits above; sticks -127..127 (up positive); triggers 0..255. */
  private static native void nativeTouchPad(int buttons, int stickX, int stickY, int cX, int cY, int trigL,
                                            int trigR);

  private static final int KIND_BUTTON = 0, KIND_STICK = 1, KIND_CSTICK = 2, KIND_DPAD = 3, KIND_MENU = 4;

  /** A control: a circle (centre and radius in units of the shorter side, x from the left or the right edge). */
  private static final class Control {
    final int kind, bit;
    final String label;
    final float ux, uy, ur;
    final boolean fromRight;
    float cx, cy, r;  // pixels
    Control(int kind, int bit, String label, boolean fromRight, float ux, float uy, float ur) {
      this.kind = kind;
      this.bit = bit;
      this.label = label;
      this.fromRight = fromRight;
      this.ux = ux;
      this.uy = uy;
      this.ur = ur;
    }
  }

  private final Control[] mControls = {
      new Control(KIND_STICK, 0, "", false, 0.27f, 0.58f, 0.18f),
      new Control(KIND_DPAD, 0, "", false, 0.62f, 0.86f, 0.12f),
      new Control(KIND_BUTTON, L, "L", false, 0.13f, 0.12f, 0.085f),
      new Control(KIND_BUTTON, R, "R", true, 0.13f, 0.12f, 0.085f),
      new Control(KIND_BUTTON, Z, "Z", true, 0.34f, 0.12f, 0.07f),
      new Control(KIND_BUTTON, A, "A", true, 0.27f, 0.58f, 0.095f),
      new Control(KIND_BUTTON, B, "B", true, 0.44f, 0.70f, 0.065f),
      new Control(KIND_BUTTON, X, "X", true, 0.10f, 0.52f, 0.065f),
      new Control(KIND_BUTTON, Y, "Y", true, 0.25f, 0.38f, 0.065f),
      new Control(KIND_CSTICK, 0, "C", true, 0.62f, 0.86f, 0.10f),
      new Control(KIND_BUTTON, START, "START", false, -1f, 0.92f, 0.06f),  // ux < 0: the middle
      new Control(KIND_MENU, 0, "MENU", false, -1f, 0.08f, 0.06f),
  };

  // Each pointer's control (index into mControls, -1 none)
  private final int[] mPointerControl = new int[16];
  private int mButtons, mStickX, mStickY, mCX, mCY;
  private int mSent = -1, mSentStick = 0, mSentC = 0;
  private boolean mShown;
  private final Paint mFill = new Paint(Paint.ANTI_ALIAS_FLAG), mLine = new Paint(Paint.ANTI_ALIAS_FLAG),
                      mText = new Paint(Paint.ANTI_ALIAS_FLAG);
  private final InputManager mInput;
  private final Handler mHandler = new Handler(Looper.getMainLooper());

  public SkiesTouchControls(Context context) {
    super(context);
    java.util.Arrays.fill(mPointerControl, -1);
    mLine.setStyle(Paint.Style.STROKE);
    mText.setTextAlign(Paint.Align.CENTER);
    mText.setTypeface(Typeface.DEFAULT_BOLD);
    mInput = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
    mInput.registerInputDeviceListener(this, mHandler);
    // Always VISIBLE (an invisible view gets no touches, and a touch shows them again): hidden just isn't drawn
    mShown = !ControllerConnected();
  }

  public void release() {
    mInput.unregisterInputDeviceListener(this);
  }

  /** A real controller (not the touchscreen, not a keyboard), external or built in. */
  private static boolean IsController(InputDevice device) {
    if (device == null || device.isVirtual())
      return false;
    final int sources = device.getSources();
    return (sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
           (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
  }

  private boolean ControllerConnected() {
    for (int id : mInput.getInputDeviceIds()) {
      if (IsController(mInput.getInputDevice(id)))
        return true;
    }
    return false;
  }

  /** From the activity: a controller was used, so the touch controls step aside until the screen is touched. */
  public void onControllerInput() {
    if (mShown)
      setShown(false);
  }

  private void setShown(boolean shown) {
    mShown = shown;
    if (!shown)
      releaseAll();
    invalidate();
  }

  @Override
  public void onInputDeviceAdded(int deviceId) {
    if (IsController(mInput.getInputDevice(deviceId)))
      setShown(false);
  }

  @Override
  public void onInputDeviceRemoved(int deviceId) {
    if (!ControllerConnected())
      setShown(true);
  }

  @Override
  public void onInputDeviceChanged(int deviceId) {
  }

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh) {
    final float unit = Math.min(w, h);
    for (Control c : mControls) {
      c.r = c.ur * unit;
      // y in units from the top; in portrait (the shorter side across) the lower controls go to the bottom
      c.cy = c.uy < 0.3f ? c.uy * unit : c.uy * unit + (h - unit);
      // the D-pad and C-stick no further in than 30% of the width, so on a squarer screen they don't meet
      final float in = c.kind == KIND_DPAD || c.kind == KIND_CSTICK ? Math.min(c.ux * unit, 0.3f * w) : c.ux * unit;
      c.cx = c.ux < 0 ? w / 2.0f : c.fromRight ? w - in : in;
    }
    mLine.setStrokeWidth(Math.max(2f, unit * 0.006f));
    mText.setTextSize(unit * 0.05f);
  }

  private int hit(float x, float y) {
    int best = -1;
    float bestDistance = Float.MAX_VALUE;
    for (int i = 0; i < mControls.length; ++i) {
      final Control c = mControls[i];
      final float dx = x - c.cx, dy = y - c.cy, d = (float) Math.sqrt(dx * dx + dy * dy);
      // a little past the drawn edge counts, the nearest control wins
      final float reach = c.kind == KIND_BUTTON || c.kind == KIND_MENU ? c.r * 1.35f : c.r * 1.2f;
      if (d < reach && d / reach < bestDistance) {
        best = i;
        bestDistance = d / reach;
      }
    }
    return best;
  }

  @Override
  public boolean onTouchEvent(MotionEvent event) {
    if (!mShown) {
      // the first touch after a controller shows the controls, and doesn't press anything
      if (event.getActionMasked() == MotionEvent.ACTION_DOWN)
        setShown(true);
      return true;
    }
    final int action = event.getActionMasked();
    final int index = event.getActionIndex();
    switch (action) {
    case MotionEvent.ACTION_DOWN:
    case MotionEvent.ACTION_POINTER_DOWN: {
      final int id = event.getPointerId(index);
      if (id < mPointerControl.length) {
        final int control = hit(event.getX(index), event.getY(index));
        mPointerControl[id] = control;
        if (control >= 0) {
          if (mControls[control].kind == KIND_MENU) {
            performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
            SkiesGameActivity.nativeBack();  // the in-game menu, as Back
          } else if (mControls[control].kind == KIND_BUTTON) {
            performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
          }
        }
      }
      break;
    }
    case MotionEvent.ACTION_UP:
    case MotionEvent.ACTION_POINTER_UP: {
      final int id = event.getPointerId(index);
      if (id < mPointerControl.length)
        mPointerControl[id] = -1;
      break;
    }
    case MotionEvent.ACTION_CANCEL:
      java.util.Arrays.fill(mPointerControl, -1);
      break;
    default:
      break;
    }
    // Everything again from the pointers still down: a finger sliding from one face button to the next moves the
    // press with it (the sticks keep the finger they started with)
    int buttons = 0, sx = 0, sy = 0, cx = 0, cy = 0;
    for (int p = 0; p < event.getPointerCount(); ++p) {
      final int id = event.getPointerId(p);
      if (id >= mPointerControl.length)
        continue;
      if ((action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_UP) && p == index)
        continue;
      int control = mPointerControl[id];
      if (control < 0)
        continue;
      final float x = event.getX(p), y = event.getY(p);
      Control c = mControls[control];
      if (c.kind == KIND_BUTTON) {
        final int now = hit(x, y);
        if (now >= 0 && mControls[now].kind == KIND_BUTTON && now != control) {
          mPointerControl[id] = control = now;
          c = mControls[now];
          performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        }
        buttons |= c.bit;
      } else if (c.kind == KIND_STICK || c.kind == KIND_CSTICK) {
        float dx = (x - c.cx) / c.r, dy = (c.cy - y) / c.r;
        final float len = (float) Math.sqrt(dx * dx + dy * dy);
        if (len > 1f) {
          dx /= len;
          dy /= len;
        }
        if (c.kind == KIND_STICK) {
          sx = Math.round(dx * 127);
          sy = Math.round(dy * 127);
        } else {
          cx = Math.round(dx * 127);
          cy = Math.round(dy * 127);
        }
      } else if (c.kind == KIND_DPAD) {
        final float dx = x - c.cx, dy = c.cy - y;
        if (Math.sqrt(dx * dx + dy * dy) > c.r * 0.25f) {
          // eight directions: a diagonal within 22.5 degrees of 45
          final double angle = Math.toDegrees(Math.atan2(dy, dx));
          final double a = angle < 0 ? angle + 360 : angle;
          if (a < 67.5 || a >= 292.5)
            buttons |= RIGHT;
          if (a >= 22.5 && a < 157.5)
            buttons |= UP;
          if (a >= 112.5 && a < 247.5)
            buttons |= LEFT;
          if (a >= 202.5 && a < 337.5)
            buttons |= DOWN;
        }
      }
    }
    update(buttons, sx, sy, cx, cy);
    return true;
  }

  private void releaseAll() {
    java.util.Arrays.fill(mPointerControl, -1);
    update(0, 0, 0, 0, 0);
  }

  private void update(int buttons, int sx, int sy, int cx, int cy) {
    final int stick = (sx << 8) | (sy & 0xFF), c = (cx << 8) | (cy & 0xFF);
    mStickX = sx;
    mStickY = sy;
    mCX = cx;
    mCY = cy;
    if (buttons == mSent && stick == mSentStick && c == mSentC)
      return;
    final boolean redraw = buttons != mButtons || stick != mSentStick || c != mSentC;
    mButtons = buttons;
    mSent = buttons;
    mSentStick = stick;
    mSentC = c;
    // L and R fully in: the analog pull with the click, as a GameCube trigger pressed to the end
    nativeTouchPad(buttons, sx, sy, cx, cy, (buttons & L) != 0 ? 255 : 0, (buttons & R) != 0 ? 255 : 0);
    if (redraw)
      invalidate();
  }

  @Override
  protected void onDraw(Canvas canvas) {
    if (!mShown)
      return;
    for (Control c : mControls) {
      final boolean pressed;
      switch (c.kind) {
      case KIND_BUTTON:
        pressed = (mButtons & c.bit) != 0;
        drawCircle(canvas, c.cx, c.cy, c.r, pressed);
        drawLabel(canvas, c.label, c.cx, c.cy);
        break;
      case KIND_MENU: {
        drawCircle(canvas, c.cx, c.cy, c.r, false);
        // three lines
        final float w = c.r * 0.5f, gap = c.r * 0.28f;
        mLine.setAlpha(170);
        for (int i = -1; i <= 1; ++i)
          canvas.drawLine(c.cx - w, c.cy + i * gap, c.cx + w, c.cy + i * gap, mLine);
        break;
      }
      case KIND_STICK:
      case KIND_CSTICK: {
        final int x = c.kind == KIND_STICK ? mStickX : mCX, y = c.kind == KIND_STICK ? mStickY : mCY;
        drawCircle(canvas, c.cx, c.cy, c.r, false);
        final float kx = c.cx + x / 127f * c.r * 0.6f, ky = c.cy - y / 127f * c.r * 0.6f;
        drawCircle(canvas, kx, ky, c.r * (c.kind == KIND_STICK ? 0.42f : 0.5f), x != 0 || y != 0);
        if (c.kind == KIND_CSTICK)
          drawLabel(canvas, c.label, kx, ky);
        break;
      }
      case KIND_DPAD: {
        final float arm = c.r * 0.36f;
        final int[] bits = {UP, DOWN, LEFT, RIGHT};
        final float[][] at = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        for (int i = 0; i < 4; ++i) {
          final float px = c.cx + at[i][0] * c.r * 0.64f, py = c.cy + at[i][1] * c.r * 0.64f;
          final RectF rect = new RectF(px - arm, py - arm, px + arm, py + arm);
          final boolean on = (mButtons & bits[i]) != 0;
          mFill.setARGB(on ? 150 : 60, 255, 255, 255);
          canvas.drawRoundRect(rect, arm * 0.3f, arm * 0.3f, mFill);
          mLine.setAlpha(on ? 230 : 140);
          canvas.drawRoundRect(rect, arm * 0.3f, arm * 0.3f, mLine);
        }
        break;
      }
      default:
        break;
      }
    }
  }

  private void drawCircle(Canvas canvas, float x, float y, float r, boolean pressed) {
    mFill.setARGB(pressed ? 150 : 50, 255, 255, 255);
    canvas.drawCircle(x, y, r, mFill);
    mLine.setARGB(pressed ? 230 : 140, 255, 255, 255);
    canvas.drawCircle(x, y, r, mLine);
  }

  private void drawLabel(Canvas canvas, String label, float x, float y) {
    if (label.isEmpty())
      return;
    final float size = mText.getTextSize();
    mText.setTextSize(label.length() > 1 ? size * 0.6f : size);
    mText.setARGB(200, 255, 255, 255);
    canvas.drawText(label, x, y - (mText.descent() + mText.ascent()) / 2, mText);
    mText.setTextSize(size);
  }
}
