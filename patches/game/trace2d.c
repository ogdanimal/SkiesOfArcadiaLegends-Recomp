/* Debug aid, not for play builds: records who loads a projection. widescreen.c's
 * ws_set_projection calls trace_projection (a weak symbol, absent from play builds) for every
 * GXSetProjection the game makes. Each distinct chain of the four innermost return addresses
 * (from the PowerPC stack back chain) gets an entry with the projection type, a hit count and the
 * first and last game frame it was seen in, readable with tools/drive.py
 * (peek:0x80319078:<size>, tools/readtrace.py). Write 0 to `used` to start over. Build
 * it together with widescreen.json. */

typedef unsigned int u32;

/* The low-memory gap is full: the tracer's code goes to the MetroTRK area (cave.ld). */
#pragma clang section text=".trkcave"

/* The table lives in MetroTRK's message buffers (lbl_80319078, 0x19B0 bytes of BSS, used only by
 * MetroTRK, which never starts without a debugger; see cave.ld), so it can hold deep chains. */
#define DEPTH 10
#define ENTRIES 128

struct trace2d {
  u32 magic; /* 'T2D2' */
  u32 used;
  u32 dropped; /* calls whose chain didn't fit */
  u32 pad;
  /* return addresses 1-DEPTH; orthographic << 31 | hits; first frame << 16 | last frame (low 16
   * bits of the game's counter) */
  u32 entries[ENTRIES][DEPTH + 2];
};

#define trace2d (*(struct trace2d *)0x80319078)
#define TRACE_MAGIC 0x54324432

/* A stack frame the back chain may point at: word-aligned, inside the 24 MB of RAM. */
static int in_ram(const u32 *p) {
  return ((u32)p & 3) == 0 && (u32)p >= 0x80000000u && (u32)p < 0x81800000u;
}

void trace_projection(const float matrix[4][4], int type) {
  u32 *frame = __builtin_frame_address(0);
  u32 lr[DEPTH];
  u32 i, k, frame_no = *(volatile u32 *)0x803475C0 & 0xFFFF; /* the game's frame counter */
  (void)matrix;
  for (k = 0; k < DEPTH; k++) /* not '= {0}': that's a call to memset */
    lr[k] = 0;
  if (trace2d.magic != TRACE_MAGIC || trace2d.used > ENTRIES) {
    /* A savestate from another build brought its own data back. */
    trace2d.magic = TRACE_MAGIC;
    trace2d.used = 0;
    trace2d.dropped = 0;
  }
  /* Each frame's back chain leads to the caller's frame, whose second word holds the return
   * address saved by the function the caller called. Skip ws_set_projection's own frame: the
   * first address kept is the return into the game's projection loader. Return addresses into
   * patch code are left out. */
  frame = (u32 *)frame[0];
  for (i = 0; i < DEPTH && in_ram(frame);) {
    frame = (u32 *)frame[0];
    if (!in_ram(frame))
      break;
    if (frame[1] >= 0x80003000) /* skip patch code (the wrappers) */
      lr[i++] = frame[1];
  }
  for (i = 0; i < trace2d.used; i++) {
    u32 *e = trace2d.entries[i];
    for (k = 0; k < DEPTH && e[k] == lr[k]; k++)
      ;
    if (k == DEPTH) {
      e[DEPTH]++;
      e[DEPTH + 1] = (e[DEPTH + 1] & 0xFFFF0000u) | frame_no;
      return;
    }
  }
  if (i < ENTRIES) {
    u32 *e = trace2d.entries[i];
    for (k = 0; k < DEPTH; k++)
      e[k] = lr[k];
    e[DEPTH] = (type ? 0x80000000u : 0) | 1;
    e[DEPTH + 1] = frame_no << 16 | frame_no;
    trace2d.used++;
  } else {
    trace2d.dropped++;
  }
}
