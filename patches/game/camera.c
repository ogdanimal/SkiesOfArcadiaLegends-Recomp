/* Player camera control for Skies of Arcadia Legends (GEAE8P, USA Rev 0): tilt, zoom and (on the
 * world map) orbit, on top of the game's own camera.
 *
 * Built into the DOL with widescreen.c by tools/gamepatch.py (spec: widescreen.json). The camera
 * research behind it: ../SkiesOfArcadiaLegends-Decomp/docs/camera.md.
 *
 * The game's camera manager (lbl_80311970, updated by fn_8022D384) in mode 2 copies the eye and the
 * angles of the field camera object (0x80302F18, shared by towns, dungeons and the world map) every
 * frame, then calls fn_8022D1F8 (camera shake) on the view's eye and angles, and builds the view
 * direction and matrix from them. cam_view wraps that call: it turns the eye around the object's
 * look-at point (Vyse, or the ship) by the player's offsets first. The game's camera object is never
 * written, so its own logic (and movement relative to the camera) is unchanged, and with the offsets
 * at rest nothing moves at all.
 *
 * The offsets come from the frontend, which reads the controller (hold Z: C-stick and triggers) and
 * writes cam_input at a fixed address (see cave.ld) every poll; with no frontend, magic stays 0 and
 * the hook does nothing. */

typedef unsigned int u32;

#define TRK_CAVE __attribute__((section(".trkcave")))

/* Ninja angles: 0x10000 = 360 degrees. */
extern float njSin(int angle);
extern float njCos(int angle);
/* Camera shake: adds the manager's shake offsets to eye. */
extern void fn_8022D1F8(float *eye, u32 *manager, int *angles);

#define CAM_MAGIC 0x43414D43 /* 'CAMC' */
#define CAM_ENABLED 1

struct cam_input {
  u32 magic;     /* 'CAMC' once the frontend has written it */
  u32 flags;     /* CAM_ENABLED */
  int pitch_off; /* added to the pitch (negative: the camera rises and looks down more) */
  int yaw_off;   /* added to the yaw (turns the camera around the look-at point) */
  float zoom;    /* distance to the look-at point, times the game's */
  int pitch_lo;  /* the tilted pitch stays in [pitch_lo, pitch_hi] (signed, Ninja units), or */
  int pitch_hi;  /*   between that and the game's own pitch if the game is already outside */
  float rise;    /* > 0: the eye stays at most this far above the game's own eye (rooms have
                  * ceilings and the game has no camera collision); tilting further brings the
                  * camera closer instead */
  u32 applied;   /* frames the offsets were applied (for checks) */
};

__attribute__((section(".cam_input"), used)) struct cam_input cam_input;

/* The manager (lbl_80311970): +0 mode, +8 pointer to the eye it copies, +0x10 eye, +0x1C angles. */
#define MANAGER_MODE 0
#define MANAGER_EYE_SOURCE 2
#define MANAGER_EYE 4
#define MANAGER_ANGLES 7
/* The field camera object: eye at +0x7C (the manager's source), look-at at +0x24. */
#define FIELD_CAMERA_EYE 0x80302F94
#define GAME_MODE (*(volatile u32 *)0x803475CC) /* 6 = field / world map */
#define FIELD_CAMERA_LOOK_AT ((const float *)0x80302F3C)

TRK_CAVE static float cam_sqrt(float x) {
  if (x <= 0.0f)
    return 0.0f;
  union {
    float f;
    u32 i;
  } u = {x};
  u.i = 0x5F3759DF - (u.i >> 1);
  float y = u.f;
  for (int i = 0; i < 3; i++)
    y = y * (1.5f - 0.5f * x * y * y);
  return x * y;
}

TRK_CAVE static void cam_orbit(float *eye, u32 *manager, int *angles) {
  const float *look_at = FIELD_CAMERA_LOOK_AT;
  const float dx = eye[0] - look_at[0], dy = eye[1] - look_at[1], dz = eye[2] - look_at[2];
  const float distance = cam_sqrt(dx * dx + dy * dy + dz * dz);
  if (distance < 1.0f) /* first-person look: the eye is at the look-at point */
    return;

  const int pitch = (short)angles[0];
  int lo = cam_input.pitch_lo, hi = cam_input.pitch_hi;
  if (pitch < lo)
    lo = pitch;
  if (pitch > hi)
    hi = pitch;
  int new_pitch = pitch + cam_input.pitch_off;
  if (new_pitch < lo)
    new_pitch = lo;
  if (new_pitch > hi)
    new_pitch = hi;
  const int new_yaw = angles[1] + cam_input.yaw_off;
  float new_distance = distance * cam_input.zoom;
  float sin_pitch = njSin(new_pitch);
  if (cam_input.rise > 0.0f && sin_pitch < 0.0f) {
    const float highest = eye[1] + cam_input.rise - look_at[1];
    if (highest <= 0.0f) {
      /* The game's eye is below the look-at point by more than rise: no eye above it stays under
       * the limit (a negative distance would put the eye on the far side), so keep the game's tilt */
      new_pitch = pitch;
      sin_pitch = njSin(new_pitch);
    } else if (new_distance * -sin_pitch > highest)
      new_distance = highest / -sin_pitch;
  }

  /* The view looks along (-sin yaw cos pitch, sin pitch, -cos yaw cos pitch) at the look-at point. */
  const float horizontal = new_distance * njCos(new_pitch);
  eye[0] = look_at[0] + horizontal * njSin(new_yaw);
  eye[1] = look_at[1] - new_distance * sin_pitch;
  eye[2] = look_at[2] + horizontal * njCos(new_yaw);
  angles[0] = new_pitch;
  angles[1] = new_yaw;

  /* Keep the manager's own copy in step (other code reads the camera's position from it). */
  float *manager_eye = (float *)(manager + MANAGER_EYE);
  manager_eye[0] = eye[0];
  manager_eye[1] = eye[1];
  manager_eye[2] = eye[2];
  manager[MANAGER_ANGLES] = (u32)new_pitch;
  manager[MANAGER_ANGLES + 1] = (u32)new_yaw;
  cam_input.applied++;
}

/* fn_8022D384 -> fn_8022D1F8 (0x8022D910): eye = the view's eye (lbl_803119E0), angles = pitch,
 * yaw, roll on the caller's stack, from which the view direction is built next. */
TRK_CAVE void cam_view(float *eye, u32 *manager, int *angles) {
  /* Field only: the frontend stops updating cam_input outside the field, so in a battle's
   * intro the last offsets would otherwise still apply. */
  if (cam_input.magic == CAM_MAGIC && (cam_input.flags & CAM_ENABLED) && GAME_MODE == 6 &&
      manager[MANAGER_MODE] == 2 && manager[MANAGER_EYE_SOURCE] == FIELD_CAMERA_EYE &&
      (cam_input.pitch_off != 0 || cam_input.yaw_off != 0 || cam_input.zoom != 1.0f))
    cam_orbit(eye, manager, angles);
  fn_8022D1F8(eye, manager, angles);
}
