/* Widescreen for Skies of Arcadia Legends (GEAE8P, USA Rev 0).
 *
 * Built into the DOL by tools/gamepatch.py (spec: widescreen.json); it runs as game code, so the
 * recompiled module, the Android build and the interpreter all get it. How the game's projection
 * works, and why these addresses: ../SkiesOfArcadiaLegends-Decomp/docs/widescreen.md.
 *
 * 3D: the Ninja screen's x aspect (set once at boot by njSetAspect(1.1, 1.0)) is scaled so the
 *     horizontal field of view fills the wider picture (Hor+). Culling and njProjectScreen read the
 *     same numbers, so they widen with it.
 * 2D: the orthographic projection's x scale is narrowed so the HUD keeps its shape, centred in a
 *     4:3 area. Full-screen 2D has to stay full width: the Ninja polygon calls (ws_poly,
 *     ws_tex_poly) switch to it for any polygon that spans the screen from edge to edge
 *     (backgrounds, overlays, fades), and the Ninja screen-copy and fade effects and the script
 *     fade always use it (the wrappers in fullwidth.S). While the squeezed 2D projection is loaded, drawing is clipped
 *     to the 4:3 area (ws_set_projection): the game parks UI it isn't showing yet just off-screen
 *     (next menu pages, cursor arms sliding in), which would otherwise show at the sides.
 * HUD anchors: code ranges listed in ws_config.anchors draw their 2D moved toward a wide edge
 *     and/or up or down (the battle HUD's panels, ws_anchor_find), unclipped. Text is queued and
 *     drawn later, so it's moved when it's queued (ws_text_anchor) and drawn unclipped then. The
 *     edges are the picture's, or those of a narrower centred area the player picks (ws_hud_aspect).
 *
 * ws_config sits at a fixed address (the start of the data section, see cave.ld) so a frontend can
 * change the target aspect at runtime with a memory write; ws_frame_end applies it every frame. */

typedef unsigned int u32;

/* Ninja screen parameters (lbl_80345878) and view volume (lbl_80345850). */
#define NJ_SCREEN ((volatile float *)0x80345878)
#define NJ_SCALE_X NJ_SCREEN[0]     /* aspect_x * distance */
#define NJ_INV_SCALE_X NJ_SCREEN[4] /* 1 / NJ_SCALE_X */
#define NJ_DISTANCE NJ_SCREEN[6]
#define NJ_CENTER_X NJ_SCREEN[2]
#define NJ_ASPECT_X NJ_SCREEN[7]
#define NJ_WIDTH NJ_SCREEN[10]
#define NJ_VOLUME ((volatile float *)0x80345850) /* [0] far, [1] near (both stored positive) */
/* The 2D orthographic projection (lbl_803463C0); fn_8029CB74 loads it before every 2D draw. */
#define ORTHO_M00 (*(volatile float *)0x803463C0)

/* The game's x aspect, 704/640: its 640-pixel line spans the analog 704-pixel width. */
#define GAME_ASPECT_X 1.1f
/* So the unpatched picture's display aspect is 4:3 * 640/704. */
#define GAME_DISPLAY_ASPECT (4.0f / 3.0f * 640.0f / 704.0f)

extern void njClipZ(float near, float far);
/* Ninja 2D triangle strips in screen pixels: x, y, z, colour (0x10 bytes) and x, y, z, u, v,
 * colour (0x18 bytes) per vertex. */
extern void fn_80290ED4(const float *vertices, int count, int mode);
extern void fn_80291064(const float *vertices, int count, int mode);
extern void GXSetProjection(const float matrix[4][4], int type);
extern void GXSetScissor(u32 left, u32 top, u32 width, u32 height);
#define ORTHO_MATRIX ((const float(*)[4])0x803463C0)

/* Projects a 3D point to screen pixels with the given matrix (NULL: the current one). */
extern void njProjectScreen(void *matrix, const float *point, float *screen);

/* 2D drawn by code that returns into (lo, lo + size] (any caller up the stack) is moved by side /
 * 100 of the width the picture gained on each side (-100: to the left edge, 100: to the right edge)
 * plus dx HUD pixels, and by dy pixels down (negative: up). The first matching entry, innermost
 * caller first, wins; lo = 0 ends the table. Kind WS_SHIP_PLATE: a ship battle HP plate, whose
 * place depends on which ship it is (fn_8015887C: plate 0, the party's ship, at the left; the
 * enemies' in a column at the right): the entry's move is for the enemies', plate 0 mirrors its
 * side and keeps its height. */
struct ws_anchor {
  u32 lo;
  unsigned short size;
  signed char side;
  unsigned char kind;
  short dx;
  short dy;
};

#define WS_SHIP_PLATE 1
/* Kind WS_SHIP_POPUP: a popup (damage numbers, fn_8021F330 / fn_8021F0EC) in a ship battle, placed
 * next to a plate: right of x 400 it moves like the enemies' plates, left of 240 like the party's,
 * anything between stays (and outside ship battles nothing moves: the popups are shared code). */
#define WS_SHIP_POPUP 2

/* The plate object fn_8015887C is drawing and the popup sprite being drawn (set by the stubs in
 * fullwidth.S). */
void *ws_ship_plate;
float *ws_popup_sprite;

#define GAME_MODE (*(volatile u32 *)0x803475CC)
#define MAP_ID (*(volatile int *)0x80311AC4)

#define WS_ANCHORS 18

struct ws_config {
  u32 magic;            /* 'WSCF' */
  u32 version;          /* WS_VERSION */
  float display_aspect; /* target picture aspect (16:9 = 1.7778); 0 = the original 4:3 */
  float full_m00;       /* full-width ortho x scale, used by fullwidth.S */
  float applied_aspect; /* NJ_ASPECT_X as last written by ws_apply */
  float factor;         /* HUD x scale: GAME_DISPLAY_ASPECT / display_aspect (1 = no change) */
  struct ws_anchor anchors[WS_ANCHORS];
};

/* Bumped whenever the data section's layout or the defaults change. A savestate made with another
 * build brings that build's data section back (its code is replaced by this build's on load), so
 * ws_check resets everything here when the version in memory isn't this one. */
#define WS_VERSION 10

/* The HUD's width, written by the frontend (Core/SkiesSettings.cpp) at a fixed address (cave.ld;
 * unused in every earlier build, so it writes it whatever the DOL): 0 = the anchored pieces at the
 * picture's edges; > 0 = at the edges of a centred area of that aspect (a 16:9 HUD on a 21:9 or
 * 32:9 picture), never wider than the picture; < 0 = the original layout, nothing anchored (the
 * game's own HUD in the middle 4:3, as before ws9). Not in ws_config: growing it would move every
 * variable after it, which a state made with an earlier build still has at the old addresses. */
__attribute__((section(".ws_hud"), used)) float ws_hud_aspect;

/* The HUD at the wide edges (found with trace2d.c's call chains, docs/development-notes.md *Widescreen*). A range
 * is a whole function, lo = start and size = its size, unless it says call sites. */
#define WS_HUD_ANCHORS                                                                           \
  {                                                                                              \
    {0x80022F74, 0x5F8, -100, 0, 0, 0},   /* battle: party status panels (fn_80022F74) */          \
    {0x80025624, 0x14FC, -100, 0, 0, 0},  /* battle: command wheel (fn_80025624) */                \
    {0x800240EC, 0x5A0, -100, 0, 0, 0},   /* battle: the wheel's lists (fn_800240EC) */            \
    {0x80027C04, 0xF8C, 0, 0, 0, -16},    /* battle: SP gauge, up a little (fn_80027C04) */        \
    {0x8002B584, 0x3EC, 100, 0, 0, 0},    /* battle: status icon column */                         \
    {0x8012B054, 0x184, -100, 0, 0, 0},   /* ship battle: Spirit gauge and bar (fn_8012B054) */    \
    {0x8012C744, 0xA4, -100, 0, 0, 0},    /* ship battle: command ring (fn_8012C744) */            \
    {0x8012E640, 0xAC, -100, 0, 0, 50},   /* ship battle: the ring's lists, down by the ring */  \
    {0x801331AC, 0x2CC, -100, 0, 0, -24}, /* ship battle: crew and ship panels, midway between the \
                                             Spirit gauge and the ring (fn_801331AC) */          \
    {0x801303D0, 0x214, 100, 0, 175, -34}, /* ship battle: turn bar, top right (fn_801303D0) */    \
    {0x80131734, 0xEC, 100, 0, 175, -34}, /* ship battle: command grid, top right (fn_80131734) */ \
    {0x8015887C, 0x140, 100, WS_SHIP_PLATE, 0, 88}, /* ship battle: HP plates and icons, enemies' \
                                                       below the grid (fn_8015887C) */             \
    {0x8021F330, 0x578, 100, WS_SHIP_POPUP, 0, 88}, /* ship battle: popups by the plates */        \
    {0x8021F0EC, 0x244, 100, WS_SHIP_POPUP, 0, 88}, /* ship battle: popups by the plates */        \
    {0x8016B434, 0xBC, -100, 0, 0, 0},    /* world map: altitude gauge (fn_8016B434) */            \
    {0x8016C204, 0xBC, 100, 0, 0, 0},     /* world map: compass (fn_8016C204) */                   \
    {0x800E26F8, 0xCC, 100, 0, 0, 0},     /* dungeon minimap: fn_800E23B0's calls to it */         \
    {0x801326B0, 0x980, 100, 0, 0, 0},    /* field party status */                                 \
  }

/* Defaults for a 640-wide screen, aiming at 16:9. factor starts at 1 (nothing widened yet): until
 * the first ws_apply at the end of a frame widens the view, RAM holds the game's own projection (a
 * fresh boot, or a state whose patch data the load replaced with these), and the hooks must treat
 * it as 4:3. Starting at the 16:9 factor gave that first frame the widescreen HUD path, which in a
 * 4:3 battle left faint differences to the unpatched game for good (Windows dumps, 2026-10-10). */
static const struct ws_config ws_defaults = {
    0x57534346, WS_VERSION, 16.0f / 9.0f, 2.0f / 640.0f, 0.0f, 1.0f, WS_HUD_ANCHORS};

__attribute__((section(".ws_config"), used)) struct ws_config ws_config = {
    0x57534346, WS_VERSION, 16.0f / 9.0f, 2.0f / 640.0f, 0.0f, 1.0f, WS_HUD_ANCHORS};

/* The scissor the game asked for (it only ever sets the whole screen). */
static u32 game_scissor[4] = {0, 0, 640, 480};

/* Text nodes moved by ws_text_anchor this frame. */
#define TEXT_ANCHORED 32
static void *text_anchored[TEXT_ANCHORED];
static int text_anchored_count;

static void unwide_reset(void);

static void ws_check(void) {
  if (ws_config.magic == ws_defaults.magic && ws_config.version == WS_VERSION)
    return;
  ws_config = ws_defaults;
  unwide_reset();
  game_scissor[0] = 0;
  game_scissor[1] = 0;
  game_scissor[2] = 640;
  game_scissor[3] = 480;
  text_anchored_count = 0;
}

/* Sets Ninja's x aspect and what derives from it, and rebuilds the perspective matrix and frustum. */
static void set_aspect_x(float aspect) {
  NJ_ASPECT_X = aspect;
  NJ_SCALE_X = aspect * NJ_DISTANCE;
  NJ_INV_SCALE_X = 1.0f / NJ_SCALE_X;
  njClipZ(-NJ_VOLUME[1], -NJ_VOLUME[0]);
}

void ws_apply(void) {
  float target = ws_config.display_aspect;
  float factor = target > 0.0f ? GAME_DISPLAY_ASPECT / target : 1.0f;
  float aspect = GAME_ASPECT_X * factor;
  float full;

  ws_check();
  if (factor >= 1.0f)
    text_anchored_count = 0; /* 4:3 goes around ws_text_flush_wide (fullwidth.S) */
  if (NJ_WIDTH <= 0.0f || NJ_DISTANCE <= 0.0f)
    return; /* Ninja isn't set up yet */
  full = 2.0f / NJ_WIDTH;
  ws_config.full_m00 = full;
  ws_config.factor = factor;
  ORTHO_M00 = full * factor;

  if (aspect != ws_config.applied_aspect || aspect != NJ_ASPECT_X) {
    /* The derived scale and the frustum are only rebuilt when the game changes the screen or the
     * field of view, which a fixed cutscene camera may never do; rebuild them here. */
    set_aspect_x(aspect);
    ws_config.applied_aspect = aspect;
  }
}

/* 3D at the game's own field of view, for draws the wrappers in fullwidth.S put between
 * ws_unwide_begin and ws_unwide_end: the battle-entry shatter (fn_802297C4) draws the copied screen
 * as 3D tiles sized to the unwidened view, so with the widened projection they covered only the
 * middle 4:3 of the picture, the whole 16:9 scene squeezed into it. Ninja loads the perspective
 * matrix again for every 3D draw (fn_8029C8A0), so rebuilding it here is enough. */
static float unwide_saved[3];
static int unwide_depth;

/* An older build's state can arrive with the depth mid-count: from 0 again (ws_check). */
static void unwide_reset(void) { unwide_depth = 0; }

void ws_unwide_begin(void) {
  if (unwide_depth++ != 0 || ws_config.factor <= 0.0f || NJ_DISTANCE <= 0.0f)
    return;
  unwide_saved[0] = NJ_ASPECT_X;
  unwide_saved[1] = NJ_SCALE_X;
  unwide_saved[2] = NJ_INV_SCALE_X;
  set_aspect_x(NJ_ASPECT_X / ws_config.factor);
}

void ws_unwide_end(void) {
  /* Never below 0: an end without its begin (after unwide_reset) would leave every later pair off */
  if (unwide_depth <= 0 || --unwide_depth != 0 || ws_config.factor <= 0.0f || NJ_DISTANCE <= 0.0f)
    return;
  NJ_ASPECT_X = unwide_saved[0];
  NJ_SCALE_X = unwide_saved[1];
  NJ_INV_SCALE_X = unwide_saved[2];
  njClipZ(-NJ_VOLUME[1], -NJ_VOLUME[0]);
}

/* Whether a 2D polygon reaches both screen edges. */
static int spans_screen(const float *vertices, int count, int stride) {
  float left = 1e9f, right = -1e9f;
  int i;
  for (i = 0; i < count; i++, vertices += stride) {
    if (vertices[0] < left)
      left = vertices[0];
    if (vertices[0] > right)
      right = vertices[0];
  }
  return count > 0 && left <= 1.0f && right >= NJ_WIDTH - 1.0f;
}

void ws_poly_wide(const float *vertices, int count, int mode) {
  float saved = ORTHO_M00;
  if (spans_screen(vertices, count, 4))
    ORTHO_M00 = ws_config.full_m00;
  fn_80290ED4(vertices, count, mode);
  ORTHO_M00 = saved;
}

void ws_tex_poly_wide(const float *vertices, int count, int mode) {
  float saved = ORTHO_M00;
  if (spans_screen(vertices, count, 6))
    ORTHO_M00 = ws_config.full_m00;
  fn_80291064(vertices, count, mode);
  ORTHO_M00 = saved;
}

/* For 2D drawn at a projected 3D point (battle name labels, HP bars, damage numbers): the HUD's
 * squeezed x scale would pull it toward the centre, so return x in HUD units, which is where the
 * unwidened projection put it. Only for callers that place 2D with the result; callers that test
 * the result against the screen bounds (culling) keep the real, widened projection. */
void ws_project_hud_wide(void *matrix, const float *point, float *screen) {
  njProjectScreen(matrix, point, screen);
  screen[0] = NJ_CENTER_X + (screen[0] - NJ_CENTER_X) / ws_config.factor;
}

void ws_set_scissor(u32 left, u32 top, u32 width, u32 height) {
  ws_check();
  game_scissor[0] = left;
  game_scissor[1] = top;
  game_scissor[2] = width;
  game_scissor[3] = height;
  GXSetScissor(left, top, width, height);
}

/* The 4:3 area in screen pixels, rounded outwards, inside the game's scissor. */
static void set_clip(void) {
  float half = 0.5f * NJ_WIDTH * ws_config.factor;
  int left = (int)(NJ_CENTER_X - half), right = (int)(NJ_CENTER_X + half + 0.999f);
  int game_left = (int)game_scissor[0], game_right = (int)(game_scissor[0] + game_scissor[2]);
  if (left < game_left)
    left = game_left;
  if (right > game_right)
    right = game_right;
  if (right < left)
    right = left;
  GXSetScissor((u32)left, game_scissor[1], (u32)(right - left), game_scissor[3]);
}

static void set_game_scissor(void) {
  GXSetScissor(game_scissor[0], game_scissor[1], game_scissor[2], game_scissor[3]);
}

/* A stack frame the back chain may point at: word-aligned, inside the 24 MB of RAM. */
static int in_ram(const u32 *p) {
  return ((u32)p & 3) == 0 && (u32)p >= 0x80000000u && (u32)p < 0x81800000u;
}

/* Looks for an anchored caller up the stack (PowerPC back chain: each frame's first word points
 * to the caller's frame, whose second word holds the return address into the caller's caller).
 * Returns whether one matched, with the move in screen pixels. */
/* The anchor code lives in the MetroTRK area (cave.ld): the low-memory gap is full. */
#define TRK_CAVE __attribute__((section(".trkcave")))

TRK_CAVE static int ws_anchor_find(float *dx, float *dy) {
  u32 *frame = __builtin_frame_address(0);
  float hud_factor;
  int depth, i;
  ws_check();
  /* The original 4:3 picture keeps the game's own HUD: no move (the gauge's dy would still apply)
   * and no unclipped text. */
  if (ws_config.anchors[0].lo == 0 || ws_config.factor >= 1.0f || ws_hud_aspect < 0.0f)
    return 0;
  /* The HUD area's x scale, like factor: never below the picture's, never above the 4:3 one. */
  hud_factor = ws_hud_aspect > 0.0f ? GAME_DISPLAY_ASPECT / ws_hud_aspect : ws_config.factor;
  if (hud_factor < ws_config.factor)
    hud_factor = ws_config.factor;
  if (hud_factor > 1.0f)
    hud_factor = 1.0f;
  for (depth = 0; depth < 12; depth++) {
    u32 lr;
    frame = (u32 *)frame[0];
    if (!in_ram(frame))
      break;
    lr = frame[1];
    for (i = 0; i < WS_ANCHORS && ws_config.anchors[i].lo != 0; i++) {
      const struct ws_anchor *a = &ws_config.anchors[i];
      if (lr > a->lo && lr <= a->lo + a->size) {
        int side = a->side, move_x = a->dx, move_y = a->dy, party = 0;
        if (a->kind == WS_SHIP_PLATE) {
          party = ws_ship_plate == 0 || *(short *)((char *)ws_ship_plate + 0x10) == 0;
        } else if (a->kind == WS_SHIP_POPUP) {
          const float x = ws_popup_sprite ? ws_popup_sprite[0] : 320.0f;
          if (GAME_MODE != 6 || MAP_ID < 500 || MAP_ID > 599 || (x >= 240.0f && x <= 400.0f))
            return 0;
          party = x < 240.0f;
        }
        if (party) {
          side = -side;
          move_x = 0;
          move_y = 0;
        }
        /* Half the HUD area's width in HUD pixels is 320 / hud_factor (the whole screen: 320 /
         * factor), of which 320 was there before. */
        *dx = side * 0.01f * (0.5f * NJ_WIDTH) * (1.0f / hud_factor - 1.0f) + move_x;
        *dy = move_y;
        return 1;
      }
    }
  }
  return 0;
}

/* Optional debug hook (trace2d.c): sees every projection load. */
extern void trace_projection(const float matrix[4][4], int type) __attribute__((weak));

/* The 2D position matrix (lbl_803464D0): screen pixels to the centred space of the projection. */
#define POS_MATRIX ((const volatile float *)0x803464D0)

/* Every projection change goes through here (Ninja's loaders call GXSetProjection), and each one
 * sets the scissor that goes with it: the 4:3 clip for the squeezed 2D projection, the game's own
 * scissor for anything else. Both are FIFO writes, so they stay together when the game records
 * the load into a display list and replays it later. Remembering whether the clip was on and
 * skipping redundant writes doesn't work: the CPU's order of calls isn't the GPU's once lists are
 * involved, and a replayed list could leave the clip on for the 3D scene (the Valuan ship's red
 * alert room was cut to 4:3 that way). Anchored HUD code gets the projection moved instead, and
 * no clip. */
void ws_set_projection(const float matrix[4][4], int type) {
  float dx, dy;
  ws_check();
  if (trace_projection)
    trace_projection(matrix, type);
  if (matrix == ORTHO_MATRIX && ws_anchor_find(&dx, &dy)) {
    float moved[4][4];
    int i, j;
    for (i = 0; i < 4; i++)
      for (j = 0; j < 4; j++)
        moved[i][j] = matrix[i][j];
    moved[0][3] += moved[0][0] * POS_MATRIX[0] * dx;
    moved[1][3] += moved[1][1] * POS_MATRIX[5] * dy;
    GXSetProjection(moved, type);
    set_game_scissor();
    return;
  }
  GXSetProjection(matrix, type);
  if (matrix == ORTHO_MATRIX && ORTHO_M00 != ws_config.full_m00)
    set_clip();
  else
    set_game_scissor();
}

/* Queued text (TextNode, decomp include/game/text.h): its position is at +0x04 / +0x08 and its
 * clip rectangle (x0, x1, y0, y1) at +0x18, moved with it (the SP gauge's "/ 8" has a tight one and
 * vanished when only the text moved). The
 * wrappers in fullwidth.S pass every node TextPrint, TextPrintN and TextPrintNumber return
 * through here; nodes queued by anchored code are moved and remembered, so ws_text_draw_node can
 * draw them without the 4:3 clip. */
TRK_CAVE void *ws_text_anchor(void *node) {
  float dx, dy;
  if (node != 0 && ws_anchor_find(&dx, &dy)) {
    float *pos = (float *)((char *)node + 4);
    float *clip = (float *)((char *)node + 0x18);
    pos[0] += dx;
    pos[1] += dy;
    clip[0] += dx;
    clip[1] += dx;
    clip[2] += dy;
    clip[3] += dy;
    if (text_anchored_count < TEXT_ANCHORED)
      text_anchored[text_anchored_count++] = node;
  }
  return node;
}

extern void TextDrawNode(void *node);
extern void TextFlush(void);

/* TextFlush's call to draw one node. */
TRK_CAVE void ws_text_draw_node_wide(void *node) {
  int i;
  ws_check();
  for (i = 0; i < text_anchored_count; i++) {
    if (text_anchored[i] == node) {
      set_game_scissor();
      TextDrawNode(node);
      set_clip();
      return;
    }
  }
  TextDrawNode(node);
}

/* main's call to draw the text queue; the queued nodes are freed by it. */
void ws_text_flush_wide(void) {
  TextFlush();
  text_anchored_count = 0;
}

/* main's call to the end-of-frame function goes to ws_frame_end (fullwidth.S), which calls
 * ws_apply and then jumps to fn_801DC420. */
