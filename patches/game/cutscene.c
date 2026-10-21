/* Cutscene framing in widescreen (issue #10) for Skies of Arcadia Legends (GEAE8P, USA Rev 0).
 *
 * Built into the DOL with widescreen.c by tools/gamepatch.py (spec: widescreen.json). The cutscenes
 * are scripted field scenes, and some shots stand on set pieces built only as wide as the 4:3
 * camera sees (the opening's look up at the Valuan ship's bridge: its side decks end in mid-air
 * past the 4:3 edges). widescreen.c widens every shot; a shot listed in cs_shot_aspect instead
 * shows no more than the picture aspect it lists, by a longer screen distance (a zoom), so the 16:9
 * picture keeps the sides that exist and loses some of the top and bottom.
 *
 * Which shot is up: the script VM (handler table lbl_802F7940, 12 bytes per opcode, the handler
 * third; a context's PC at +0x1C points at the opcode word, and the handler moves it past the
 * instruction) runs the camera commands through the stubs in cutscene.S (redirect_pointers, and
 * cs_frame every frame, because a save state brings back the table of the build it was made with). After a
 * command, the shot is the map and an FNV-1a hash of the command's words, with the camera mode
 * (lbl_80311970 +0) it left; the shot ends when another camera command runs, the map changes or
 * the camera leaves that mode (the field camera after a cutscene). Command words, unlike their
 * place in the script, are the same wherever the script moves.
 *
 * The zoom: the camera manager sets Ninja's screen every frame, through njSetScreen (distance and
 * centre) or njSetPerspective (fn_802992F8: the distance from the lens angle, fn_8028BAEC); both go
 * through cs_set_*, which keep the game's distance and centre and set the screen again with the
 * distance times the zoom (njSetScreen reads only those). Hor+ widening shows tan(half width) in proportion to the picture aspect, so a
 * shot allowed aspect A in a picture of aspect D > A takes the distance times D / A.
 *
 * Finding a shot's hash: play to it and read cs_shot (patches.txt has its address): key, map,
 * the command's offset in the script and its opcode. Poking key, map and an aspect (per mille)
 * into cs_shot.test_* frames that shot without a rebuild. */

typedef unsigned int u32;

#define TRK_CAVE __attribute__((section(".trkcave")))

/* lbl_802F7940: the script VM's handlers, 12 bytes per opcode (flags, argument info, handler). */
#define SCRIPT_HANDLERS ((volatile u32 *)0x802F7940)
#define SCRIPT_BASE (*(volatile u32 *)0x8030CEA0) /* lbl_8030CEA0 +0: the loaded script's code */
#define CAMERA_MODE (*(volatile u32 *)0x80311970) /* lbl_80311970 +0: the camera manager's mode */
#define GAME_MODE (*(volatile u32 *)0x803475CC)   /* 6 = field (towns, dungeons, cutscenes) */
#define MAP_ID (*(volatile int *)0x80311AC4)

extern void njSetScreen(const float *screen);
extern void njSetPerspective(int angle);
#define NJ_SCREEN ((volatile float *)0x80345878) /* lbl_80345878: centre at [2] / [3], distance [6] */
extern float ws_factor(void);

struct cs_shot {
  u32 key;          /* FNV-1a of the last camera command's words; 0 = none */
  int map;          /* MAP_ID when it ran */
  u32 camera_mode;  /* CAMERA_MODE after it ran */
  u32 offset;       /* its offset from the script's code (for finding it in a script dump) */
  u32 opcode;
  float zoom;       /* the zoom the screen has now (1 = none) */
  int anchor;       /* the listed shot's CS_BOTTOM / CS_TOP, 0 = centred */
  float shift;      /* what cs_apply added to the screen's centre y */
  float screen[3];  /* the game's last screen: centre x, y and distance (0: none yet) */
  u32 test_key;     /* a shot to frame without a rebuild (with test_map and test_aspect) */
  int test_map;
  int test_aspect;  /* per mille */
};

/* After every older build's data, so their states hold zeros here (cave.ld). */
__attribute__((section(".late_bss"), used)) struct cs_shot cs_shot;

/* The widest picture aspect (per mille) each listed shot can show; 0 = any. Plus CS_BOTTOM: the
 * shot keeps its bottom edge where the 4:3 picture has it and loses only the top (CS_TOP: the
 * reverse), by moving the screen's centre. An if-chain, not a table: constants would go in the full
 * low-memory gap, and a save state brings back old data. */
#define CS_BOTTOM 10000
#define CS_TOP 20000
TRK_CAVE static int cs_shot_aspect(int map, u32 key) {
  if (cs_shot.test_aspect > 0 && map == cs_shot.test_map && key == cs_shot.test_key)
    return cs_shot.test_aspect;
  if (map == 201) { /* the opening (me201a.sct) */
    if (key == 0x90763369)
      return 1360 + CS_BOTTOM; /* up at the Valuan ship's bridge: its decks end at 1.38 */
    if (key == 0xE6946063)
      return 1380; /* Fina's ship by the moon: the Valuan ship waits just past the right at 1.40 */
  }
  return 0;
}

TRK_CAVE static float cs_zoom(void) {
  const float factor = ws_factor();
  int aspect;
  float zoom;
  if (cs_shot.key == 0 || factor >= 1.0f || GAME_MODE != 6 || MAP_ID != cs_shot.map ||
      CAMERA_MODE != cs_shot.camera_mode)
    return 1.0f;
  aspect = cs_shot_aspect(cs_shot.map, cs_shot.key);
  cs_shot.anchor = aspect / CS_BOTTOM * CS_BOTTOM;
  aspect -= cs_shot.anchor;
  if (aspect <= 0)
    return 1.0f;
  /* The picture's aspect over the allowed one: widescreen.c's GAME_DISPLAY_ASPECT / factor, over
   * aspect / 1000. */
  zoom = (4.0f / 3.0f * 640.0f / 704.0f * 1000.0f) / (factor * aspect);
  return zoom > 1.0f ? zoom : 1.0f;
}

/* Sets the game's last screen again with the zoom the shot has now. */
TRK_CAVE static void cs_apply(void) {
  float screen[8];
  cs_shot.zoom = cs_zoom();
  screen[2] = cs_shot.screen[0];
  screen[3] = cs_shot.screen[1];
  screen[6] = cs_shot.screen[2] * cs_shot.zoom;
  /* The edge kept stays as far below (or above) the centre, in the scene, as before the zoom. */
  cs_shot.shift = 0.0f;
  if (cs_shot.anchor == CS_BOTTOM)
    cs_shot.shift = -cs_shot.screen[1] * (cs_shot.zoom - 1.0f);
  else if (cs_shot.anchor == CS_TOP)
    cs_shot.shift = cs_shot.screen[1] * (cs_shot.zoom - 1.0f);
  screen[3] += cs_shot.shift;
  njSetScreen(screen);
}

TRK_CAVE void cs_set_screen(const float *screen) {
  cs_shot.screen[0] = screen[2];
  cs_shot.screen[1] = screen[3];
  cs_shot.screen[2] = screen[6];
  cs_apply();
}

/* njSetPerspective leaves the centre as it is: the game's, plus the shift cs_apply made. */
TRK_CAVE void cs_set_perspective(int angle) {
  njSetPerspective(angle);
  cs_shot.screen[0] = NJ_SCREEN[2];
  cs_shot.screen[1] = NJ_SCREEN[3] - cs_shot.shift;
  cs_shot.screen[2] = NJ_SCREEN[6];
  cs_shot.zoom = 1.0f;
  if (cs_zoom() != 1.0f || cs_shot.shift != 0.0f)
    cs_apply();
}

/* After a camera command (cutscene.S): the shot it starts, if it moved the PC past itself. */
TRK_CAVE void cs_note(const unsigned char *context, u32 pc) {
  const u32 end = *(const volatile u32 *)(context + 0x1C);
  const u32 *word;
  u32 hash = 2166136261u;
  if (end <= pc || end - pc > 0x100)
    return;
  for (word = (const u32 *)pc; (u32)word < end; word++) {
    hash ^= *word;
    hash *= 16777619u;
  }
  cs_shot.key = hash ? hash : 1;
  cs_shot.map = MAP_ID;
  cs_shot.camera_mode = CAMERA_MODE;
  cs_shot.offset = pc - SCRIPT_BASE;
  cs_shot.opcode = *(const u32 *)pc;
  if (cs_shot.screen[2] > 0.0f && cs_zoom() != cs_shot.zoom)
    cs_apply();
}

/* cutscene.S: 16 bytes per stub, the first instruction li r11, opcode. */
extern const unsigned short cs_stubs[], cs_stubs_end[];

/* Once a frame, from ws_frame_end. */
TRK_CAVE void cs_frame(void) {
  const unsigned short *stub;
  for (stub = cs_stubs; stub < cs_stubs_end; stub += 8)
    if (SCRIPT_HANDLERS[stub[1] * 3 + 2] != (u32)stub)
      SCRIPT_HANDLERS[stub[1] * 3 + 2] = (u32)stub;
  if (cs_shot.screen[2] > 0.0f && GAME_MODE == 6 && cs_zoom() != cs_shot.zoom)
    cs_apply();
}
