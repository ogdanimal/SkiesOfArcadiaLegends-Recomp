/* Direct load of a memory card save at boot (Continue in the launchers): no intro, title screen or
 * load screen. The frontend writes load_request (a fixed address, cave.ld) before the title gets
 * going; at the point where the title would start the intro this reads card A's file N with the
 * game's own card code and starts the game from it the way the title's Continue does. Anything
 * that fails leaves the title to carry on as usual.
 *
 * Research: ../SkiesOfArcadiaLegends-Decomp/docs/ui.md (*Loading a save without the load screen*).
 *
 * The title (mode 3, fn_80228A20, called once a frame from 0x801DC350) is a state machine on
 * lbl_80311AD4[3]: 0 init, 1 waits for the font and then starts the intro (states 7 / 8: in-engine
 * field scenes) or, with lbl_8034782C set, the title proper (9). The load screen (SaveLoadTask)
 * loads a file in two steps: 17 calls fn_801A4070 (mounts the slot, opens "SA_LEGENDS.%03d" by the
 * work's file index, starts a 0x6000-byte CARDReadAsync into a heap buffer whose descriptor goes to
 * work +0x258) and 18 polls fn_801A3A10 (-1 busy, 0 done: file closed, card unmounted), checks the
 * payload's XOR (fn_801A4684 over buf +0x1440, 0x31FC bytes, against the word after it) and
 * applies it to the game's data (fn_801A4354), then frees the buffer (fn_801A41C0). The screen's
 * end (fn_8019E294) sets lbl_803473C4 = 1 and clears bit 31 of lbl_80310B3C +0x84. Both card
 * functions only touch the task's work (+0x24): +1 = 1 (load), +4 = slot, +7 = file index. The
 * title's Continue then sets its state 0x12 and the game mode to 6 (fn_80228AD4), after the menu
 * has set lbl_80347634 (fn_801D71BC(1)). */

typedef unsigned int u32;
typedef unsigned char u8;

#define TRK_CAVE __attribute__((section(".trkcave")))

/* The load screen's work (SaveLoadOpenLoad / SaveLoadOpenSave allocate it): 0x270 bytes on the USA
 * and European discs, 0x274 on the Japanese, whose fn_801A4070 stores the EA_LEGEND_EXTRA probe's
 * result as a word at +0x270 (review D9). gamepatch.py defines GAME_<ID>. */
#ifdef GAME_GEAJ8P
#define DL_WORK_SIZE 0x274
#else
#define DL_WORK_SIZE 0x270
#endif

extern void fn_80228A20(void);              /* the title, once a frame */
extern int FontIsReady(void);
extern int FontUpdate(void);                /* one step of the font's loading */
extern int fn_801A4070(u8 *task);           /* load: mount, open, start the read */
extern int fn_801A3A10(int slot, u8 *task); /* load: poll (-1 busy, 0 done, else a CARD error) */
extern u32 fn_801A4684(const void *p, u32 size); /* XOR of the words */
extern void fn_801A4354(u8 *payload);       /* the save's data into the game's */
extern void fn_801A41C0(u8 *desc);          /* frees the read buffer */
extern void fn_801A372C(void);              /* unmounts both slots, frees their work areas */
extern void *fn_801E1C7C(u32 size);         /* heap alloc */
extern void fn_801E17E0(void *p);           /* heap free */
extern void fn_801D71BC(int on);            /* lbl_80347634: Z counts as Start */
extern void fn_801DBE6C(int mode);          /* switch the game mode */
extern int fn_8022A854(int *state);         /* the resident files into ARAM: one step, 1 when done */

#define LOAD_MAGIC 0x4C4F4144 /* 'LOAD' */
enum { LOAD_PENDING = 0, LOAD_RUNNING = 1, LOAD_DONE = 2, LOAD_FAILED = 3 };

/* Written by the frontend (VideoCommon/SkiesMenu.cpp) at boot; status goes back to it. */
struct load_request {
  u32 magic; /* 'LOAD' for a request */
  int file;  /* 1..99: card A's SA_LEGENDS.(file - 1) */
  int status;
  int error; /* LOAD_FAILED: 1 not started (no card / no file), 2 read error, 3 bad data */
};
__attribute__((section(".load_request"), used)) struct load_request load_request;

#define TITLE_STATE (*(volatile int *)(0x80311AD4 + 0xC))
#define TITLE_LOGOS (*(volatile int *)0x80347830)   /* state 1 shows the logos (2-4) first */
#define TITLE_INTRO_DONE (*(volatile int *)0x8034782C) /* state 1 skips the intro (7 / 8) */
#define LOAD_RESULT (*(volatile int *)0x803473C4)
#define EVENT_WORD_84 (*(volatile u32 *)(0x80310B3C + 0x84))
/* The second logo (title state 4, fn_8022A38C) steps fn_8022A854 on this until it returns 1: 16
 * files (lbl_802F91F8) read into ARAM and listed in a table (lbl_803165C8, with a checksum) that the
 * battle's loading looks up (fn_8000A118 -> fn_801D67DC -> fn_801DB1A4). Without the logos the
 * table stayed empty and the first battle read 0x00000004 (ws20-ws22). */
#define RESIDENT_LOAD (*(int *)0x80347828)

/* The task the card functions are handed (only +0x24, the work, is read) and its work. */
__attribute__((section(".late_bss"))) static u8 dl_task[0x28];
__attribute__((section(".late_bss"))) static u8 *dl_work;

TRK_CAVE static void dl_fail(int error) {
  if (dl_work) {
    u8 *desc = *(u8 **)(dl_work + 0x258);
    if (desc)
      fn_801A41C0(desc);
    fn_801E17E0(dl_work);
    dl_work = 0;
  }
  fn_801A372C();
  load_request.error = error;
  load_request.status = LOAD_FAILED;
}

TRK_CAVE static int dl_same(const u8 *a, const char *b, int n) {
  for (int i = 0; i < n; i++)
    if (a[i] != (u8)b[i])
      return 0;
  return 1;
}

/* One frame of the load; nonzero while it runs (the title waits on state 1, nothing drawn). */
TRK_CAVE static int dl_step(void) {
  if (load_request.status == LOAD_PENDING) {
    if (load_request.file < 1 || load_request.file > 99) {
      load_request.error = 1;
      load_request.status = LOAD_FAILED;
      return 0;
    }
    dl_work = fn_801E1C7C(DL_WORK_SIZE);
    if (!dl_work) {
      load_request.error = 1;
      load_request.status = LOAD_FAILED;
      return 0;
    }
    for (int i = 0; i < DL_WORK_SIZE; i++)
      dl_work[i] = 0;
    for (int i = 0; i < 0x28; i++)
      dl_task[i] = 0;
    *(u8 **)(dl_task + 0x24) = dl_work;
    dl_work[1] = 1; /* load */
    dl_work[4] = 0; /* slot A */
    dl_work[7] = (u8)(load_request.file - 1);
    load_request.status = LOAD_RUNNING;
    if (!fn_801A4070(dl_task)) {
      dl_fail(1);
      return 0;
    }
    return 1;
  }
  const int result = fn_801A3A10(0, dl_task);
  if (result == -1)
    return 1;
  if (result != 0) {
    dl_fail(2);
    return 0;
  }
  u8 *desc = *(u8 **)(dl_work + 0x258);
  u8 *payload = *(u8 **)desc + 0x1440;
  /* The load screen checks the XOR; its file list (which a player picks from) also wants the
   * version string, so both here. */
  if (fn_801A4684(payload, 0x31FC) != *(u32 *)(payload + 0x31FC) ||
      !dl_same(payload, "EA_LEGEND v1.01", 16)) {
    dl_fail(3);
    return 0;
  }
  fn_801A4354(payload);
  fn_801A41C0(desc);
  fn_801E17E0(dl_work);
  dl_work = 0;
  EVENT_WORD_84 &= 0x7FFFFFFF;
  LOAD_RESULT = 1;
  load_request.status = LOAD_DONE;
  /* As after the logos, the intro and the title's Continue: the menu's Z-as-Start, then title
   * state 0x12 and the field */
  TITLE_LOGOS = 0;
  TITLE_INTRO_DONE = 1;
  fn_801D71BC(1);
  TITLE_STATE = 0x12;
  fn_801DBE6C(6);
  return 1;
}

/* 0x801DC350: the title, once a frame. With a request, state 1 is held (nothing drawn) while the
 * font and the resident files load, which the logos (states 2-4) otherwise wait out, then the load
 * runs over the next frames. A failed load lets the title go on from state 1: logos, intro, title
 * (the second logo then finds the resident files loaded). */
TRK_CAVE void dl_title(void) {
  if (load_request.magic == LOAD_MAGIC && TITLE_STATE == 1) {
    if (load_request.status == LOAD_PENDING && !FontIsReady()) {
      FontUpdate();
      return;
    }
    if (load_request.status == LOAD_PENDING && !fn_8022A854(&RESIDENT_LOAD))
      return;
    if ((load_request.status == LOAD_PENDING || load_request.status == LOAD_RUNNING) && dl_step())
      return;
  }
  fn_80228A20();
}
