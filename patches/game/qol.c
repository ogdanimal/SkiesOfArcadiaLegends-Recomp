/* Quality-of-life options for Skies of Arcadia Legends (GEAE8P, USA Rev 0): random encounter rate,
 * EXP / gold / magic EXP from battles, and battle and field speed.
 *
 * Built into the DOL with widescreen.c by tools/gamepatch.py (spec: widescreen.json). The research:
 * ../SkiesOfArcadiaLegends-Decomp/docs/qol_cheats.md. Every hook takes over a whole call:
 *
 * Encounters  fn_800C1C24 (the field's per-frame encounter check) rolls fn_8025ECC4 (rand, 0..32767)
 *             at 0x800C207C into a number in [0, 100) and starts a battle if it's below the chance.
 *             qol_encounter_rand returns the rand whose roll is roll * 100 / percent: the same as
 *             the chance times percent / 100. Off also clears the frames-since-battle counter
 *             (lbl_80346D28), so the chance stays 0. Hold B: while controller 1's B is held (the
 *             game's PADStatus buffer, *(r13 - 0x70E4) = lbl_8034763C, which it fills with PADRead
 *             every frame), the same as Off: cleartonic's "hold B" romhack, which the frontend
 *             undoes on import (it loads 0 for the counter in this function, through a cave).
 * Rewards     the ground battle results (fn_8006F4B0) read EXP, gold and magic EXP from the battle
 *             data (lbl_80347390: +0x30, +0x34, +0x01) right after counting the members with
 *             fn_80078610 (0x8006F5B8); ship battles (fn_80144E60, called at 0x8015C6B4) read EXP
 *             and gold from theirs (lbl_8034727C: +0x18, +0x1C). The data is scaled once per
 *             battle just before, so the results screen shows the numbers awarded.
 * Invincible  the party's battle records (lbl_80309DE4[0..3]: HP +0x14, max +0x18, MP +0xFE, max
 *             +0xFF) are refilled every frame in battle, and so is the Spirit gauge (battle data
 *             +0x8 / +0xA, max +0x6), so magic and S-moves cost nothing; damage to the party is
 *             zeroed in fn_8002DC14 (the damage function: HP -= action +0x8), so nothing can knock
 *             a member out. Ship battles: the ship's HP (lbl_8034727C +0x20 -> +0x18, max +0x14),
 *             the Spirit gauge (lbl_803063F0 +0x8, max +0x6) and the crew's MP (crew entries at
 *             lbl_803063F0 +0x1C6, and the party records) are refilled every frame, and the
 *             enemy's damage is zeroed in fn_8014419C (applies a phase's HP changes, then ends the
 *             battle if the ship is at 0; called at 0x80144E3C). Turning it off leaves everything
 *             as it is.
 * Speed       fn_801DC420 (end of frame) holds a frame to two retraces: a loop on
 *             VIGetRetraceCount (0x801DC498) until one has passed since the frame began (main's
 *             mark lbl_8034768C), then VIWaitForRetrace (0x801DC4D8). In battle (game mode 7, or a ship battle: field mode
 *             6 on a map from 500 to 599) at 2x the loop is skipped; 3x and 4x also skip some of
 *             the VIWaitForRetrace calls (the game draws more frames than are shown). The loop is
 *             skipped by moving the mark back before fn_801DC420 runs (qol_frame, from ws_frame_end),
 *             not by hooking its call: the game spends much of a frame polling there, and a savestate
 *             made inside patch code returns into whatever a later build put at that address.
 *             With QOL_SPEED_FIELD (ws26) the same goes for the field (game mode 6: towns, dungeons,
 *             the world map and the cutscenes, which all run there); the frontend sets it with the
 *             field speed outside battle and leaves it off in battle, so an earlier build, which
 *             reads the whole word as the battle speed, only ever sees the battle speed in battle.
 *
 * qol_input sits at a fixed address (cave.ld) and is written by the frontend every poll
 * (VideoCommon/SkiesMenu.cpp); with no frontend its magic stays 0 and the game is unchanged. */

typedef unsigned int u32;
typedef signed char s8;

#define TRK_CAVE __attribute__((section(".trkcave")))

extern int fn_8025ECC4(void);      /* rand */
extern int fn_80078610(int slot);  /* nonzero if the party slot is filled */
extern u32 fn_80144E60(u32, u32, u32, u32, u32, u32, u32, u32); /* ship battle results */
extern u32 VIGetRetraceCount(void);
extern void fn_8002DC14(unsigned char *action, int target); /* battle damage */
extern void fn_8014419C(int phase, int side); /* ship battle: a phase's HP changes */
extern void fn_801453D0(void); /* ship battle: allocates its data into lbl_8034727C */

#define QOL_MAGIC 0x514F4C43 /* 'QOLC' */

struct qol_input {
  u32 magic;         /* 'QOLC' once the frontend has written it */
  int encounters;    /* percent of the game's encounter chance (0 = none) */
  int exp;           /* percent of the EXP each battle gives */
  int gold;          /* percent of the gold */
  int magic_exp;     /* percent of the magic EXP */
  int battle_speed;  /* 1 = normal, 2..4 = that many times faster in battle (| QOL_SPEED_FIELD) */
  int invincible;    /* party HP / MP / Spirit kept full and no damage, in battle */
  int hold_b;        /* no random battle while controller 1's B is held (was padding until ws25) */
};

__attribute__((section(".qol_input"), used)) struct qol_input qol_input;

/* In qol_input.battle_speed: the speed (low byte) holds in the field too. */
#define QOL_SPEED_FIELD 0x100
#define QOL_SPEED (qol_input.battle_speed & 0xFF)

/* The battle data last scaled (cleared once the game leaves battle), so a battle is scaled once. */
/* In one struct so they keep the order (and addresses) earlier builds had: the compiler orders
 * separate statics by first use, and a save state made with an earlier build must read each one
 * where that build kept it. */
static struct {
  int speed_phase;
  u32 scaled_ground;
  u32 scaled_ship;
} qol_marks;
#define qol_speed_phase qol_marks.speed_phase
#define qol_scaled_ground qol_marks.scaled_ground
#define qol_scaled_ship qol_marks.scaled_ship
/* Nonzero from the ship battle data's allocation (fn_801453D0) until the results free it
 * (fn_80144E60). The game never clears lbl_8034727C after freeing, so from the second ship battle
 * on the pointer is set, but stale, from the map's load until the new allocation: Invincible
 * wrote through it into reused heap (ws17). In .late_bss (cave.ld), after every variable earlier
 * builds had: a state made with one reads 0 here, so the options wait for the battle's next phase
 * (qol_ship_phase). */
__attribute__((section(".late_bss"))) static int qol_ship_live;

#define GAME_MODE (*(volatile u32 *)0x803475CC) /* 6 = field / world map, 7 = battle */
extern int fn_8022A854(int *state); /* the resident files into ARAM: one step, 1 when done */
#define RESIDENT_LOAD (*(int *)0x80347828) /* fn_8022A854's state, 5 when loaded (autoload.c) */
#define RESIDENT_TABLE ((const volatile unsigned char *)0x803165C8) /* 0x204 bytes */
#define RESIDENT_SUM (*(volatile u32 *)0x80346860) /* the table's byte sum */
/* fn_8000A118's state: 0-3 load the battle (its units and data are allocated in 3, by
 * fn_80077428), 4-7 run it, 8-9 tear it down, 11-12 go back to 3. The game never clears the unit
 * pointers or the data pointer, so outside 4-7 they can still be the last battle's freed blocks. */
#define BATTLE_STATE (*(volatile u32 *)0x8034737C)
/* lbl_802DBB10, the battle action handlers' table: entry 0 is the damage function, which
 * widescreen.json's redirect_pointers points at qol_damage. It's data, so a save state brings back
 * the entry of the build it was made with (code sections are restored on load, data isn't), and
 * from ws17 on qol_damage moved: the old address then lands inside another function, and every hit
 * jumped there (ws18: "Invalid write to 0x0000001e" on the first attack). qol_frame puts it back. */
#define ACTION_HANDLERS ((void (*volatile *)(unsigned char *, int))0x802DBB10)
#define FRAMES_SINCE_BATTLE (*(volatile int *)0x80346D28)
/* The game's controller state this frame (PADStatus[4], 12 bytes each; the game swaps it with a
 * second buffer every frame): controller 1's buttons are the first halfword. */
#define PAD_STATUS (*(const volatile unsigned short *volatile *)0x8034763C)
#define PAD_BUTTON_B 0x0200
#define FRAME_START_RETRACE (*(volatile u32 *)0x8034768C) /* main's mark for fn_801DC420's loop */
#define GROUND_BATTLE_DATA (*(unsigned char *volatile *)0x80347390)
#define SHIP_BATTLE_DATA (*(unsigned char *volatile *)0x8034727C)
#define BATTLE_UNITS ((unsigned char *volatile *)0x80309DE4) /* [0..3]: the party */
#define MAP_ID (*(volatile int *)0x80311AC4)
#define SHIP_PARTY ((unsigned char *)0x803063F0) /* the party's side of a ship battle */
#define PARTY_RECORDS ((unsigned char *)0x8030B7F4) /* 0x5C each; MP +0xD, max +0xE */
#define PARTY_SLOTS ((signed char *)0x8030BB1C) /* lbl_8030B7F4 + 0x328: record index or -1 */

TRK_CAVE static int qol_on(void) { return qol_input.magic == QOL_MAGIC; }

/* A word-aligned address inside the 24 MB of main RAM: what a live battle data pointer is. */
TRK_CAVE static int qol_ram(const void *p) {
  const u32 a = (u32)p;
  return a >= 0x80000000 && a < 0x81800000 && (a & 3) == 0;
}

TRK_CAVE static int qol_scale(int value, int percent) {
  if (percent == 100 || value <= 0)
    return value;
  /* In two parts so it can't overflow (and needs no 64-bit division). */
  const int scaled = value / 100 * percent + value % 100 * percent / 100;
  if (percent > 0 && scaled == 0)
    return 1;
  return scaled > 99999999 ? 99999999 : scaled;
}

TRK_CAVE int qol_encounter_rand(void) {
  const int r = fn_8025ECC4();
  if (RESIDENT_LOAD != 5)
    return 16383; /* no battle until qol_resident has the resident files in */
  if (!qol_on())
    return r;
  const int hold_b = qol_input.hold_b && qol_ram((const void *)PAD_STATUS) &&
                     (PAD_STATUS[0] & PAD_BUTTON_B) != 0;
  if (qol_input.encounters == 100 && !hold_b)
    return r;
  if (qol_input.encounters <= 0 || hold_b) {
    FRAMES_SINCE_BATTLE = 0;
    return 16383; /* the highest roll, 99.997 */
  }
  /* The game's roll: 100 * r / 32768 - 50, wrapped into [0, 100). */
  float roll = (float)r * (100.0f / 32768.0f) - 50.0f;
  if (roll < 0.0f)
    roll += 100.0f;
  float target = roll * 100.0f / (float)qol_input.encounters;
  if (target > 99.99f)
    target = 99.99f;
  const float raw = target >= 50.0f ? target - 50.0f : target + 50.0f;
  return (int)(raw * (32768.0f / 100.0f) + 0.5f) & 0x7FFF;
}

/* 0x8006F5B8: the first call of the member count loop, right before the rewards are read. */
TRK_CAVE int qol_ground_member(int slot) {
  unsigned char *data = GROUND_BATTLE_DATA;
  if (slot == 0 && qol_on() && qol_ram(data) && qol_scaled_ground != (u32)data) {
    qol_scaled_ground = (u32)data;
    int *exp = (int *)(data + 0x30), *gold = (int *)(data + 0x34);
    *exp = qol_scale(*exp, qol_input.exp);
    *gold = qol_scale(*gold, qol_input.gold);
    const int magic = qol_scale(*(s8 *)(data + 0x01), qol_input.magic_exp);
    *(s8 *)(data + 0x01) = (s8)(magic > 127 ? 127 : magic);
  }
  return fn_80078610(slot);
}

/* 0x8015C6DC: fn_8015C6C8 allocating a ship battle's data. */
TRK_CAVE void qol_ship_alloc(void) {
  fn_801453D0();
  qol_ship_live = 1;
  qol_scaled_ship = 0;
}

/* 0x8015C6B4: ship battle results, which free the battle data at their end. fn_8012A26C makes this
 * call whenever the player leaves a field map numbered 500 or above, not only after a ship battle,
 * and the game never clears lbl_8034727C: only a battle whose data was allocated (qol_ship_live) is
 * scaled. (ws18 and before scaled through whatever the pointer held: a device wrote to 0x1A / 0x1E
 * leaving a dungeon with EXP / gold above 100%.) */
TRK_CAVE u32 qol_ship_results(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f, u32 g, u32 h) {
  unsigned char *data = SHIP_BATTLE_DATA;
  if (qol_on() && qol_ship_live && qol_ram(data) && qol_scaled_ship != (u32)data) {
    qol_scaled_ship = (u32)data;
    int *exp = (int *)(data + 0x18), *gold = (int *)(data + 0x1C);
    *exp = qol_scale(*exp, qol_input.exp);
    *gold = qol_scale(*gold, qol_input.gold);
  }
  const u32 result = fn_80144E60(a, b, c, d, e, f, g, h);
  qol_ship_live = 0;
  return result;
}

/* A ship battle runs inside field mode 6, on a map from 500 to 599 (fn_8012A26C's test), with its
 * data allocated and not yet freed. */
TRK_CAVE static int qol_ship_battle(void) {
  const int map = MAP_ID;
  return GAME_MODE == 6 && map >= 500 && map <= 599 && qol_ship_live && SHIP_BATTLE_DATA;
}

TRK_CAVE static int qol_in_battle(void) { return GAME_MODE == 7 || qol_ship_battle(); }

TRK_CAVE static int qol_invincible(void) {
  return qol_on() && qol_input.invincible && qol_in_battle();
}

/* Every frame in a ship battle: the ship's HP, the Spirit gauge and the crew's MP full. */
TRK_CAVE static void qol_refill_ship(void) {
  if (!qol_ram(SHIP_BATTLE_DATA))
    return;
  unsigned char *ship = *(unsigned char **)(SHIP_BATTLE_DATA + 0x20);
  if (qol_ram(ship) && *(int *)(ship + 0x18) > 0)
    *(int *)(ship + 0x18) = *(int *)(ship + 0x14);
  *(short *)(SHIP_PARTY + 0x8) = *(short *)(SHIP_PARTY + 0x6);
  for (int i = 0; i < 4; i++) {
    unsigned char *crew = SHIP_PARTY + 0x1C6 + i * 0x1C; /* MP +0xE, max +0xD */
    if ((signed char)crew[0xB] >= 0)
      crew[0xE] = crew[0xD];
    const int slot = PARTY_SLOTS[i];
    if (slot >= 0 && slot < 6) {
      unsigned char *record = PARTY_RECORDS + slot * 0x5C;
      record[0xD] = record[0xE];
    }
  }
}

/* Every frame in battle: the party's HP and MP (members still standing) and the Spirit gauge full. */
TRK_CAVE static void qol_refill(void) {
  if (GAME_MODE != 7) {
    qol_refill_ship();
    return;
  }
  /* (ws18 and before refilled from the battle's first frame: from the second battle on, that wrote
   * HP and the Spirit gauge into the previous battle's freed blocks while the new one loaded.) */
  if (BATTLE_STATE < 4 || BATTLE_STATE > 7)
    return;
  for (int i = 0; i < 4; i++) {
    unsigned char *unit = BATTLE_UNITS[i];
    if (!qol_ram(unit) || *(int *)(unit + 0x14) <= 0)
      continue;
    *(int *)(unit + 0x14) = *(int *)(unit + 0x18);
    unit[0xFE] = unit[0xFF];
  }
  unsigned char *data = GROUND_BATTLE_DATA;
  if (qol_ram(data)) {
    const short spirit_max = *(short *)(data + 0x6);
    *(short *)(data + 0x8) = spirit_max;
    *(short *)(data + 0xA) = spirit_max;
  }
}

/* Every call of fn_8002DC14: no damage to the party while invincible. */
TRK_CAVE void qol_damage(unsigned char *action, int target) {
  if (qol_invincible() && (short)target >= 0 && (short)target < 4) {
    const int damage = *(int *)(action + 0x8);
    *(int *)(action + 0x8) = 0;
    fn_8002DC14(action, target);
    *(int *)(action + 0x8) = damage;
    return;
  }
  fn_8002DC14(action, target);
}

/* 0x80144E3C: a ship battle phase's HP changes; side 1 is the enemy's, whose damage to the ship
 * (battle data + phase * 0x90 + 0x130) is zeroed while invincible. */
TRK_CAVE void qol_ship_phase(int phase, int side) {
  /* Only a running battle plays out phases, so its data is live: this also picks up a battle a
   * save state from an earlier build (no flag yet) was made in, from its first phase on. */
  qol_ship_live = 1;
  if (side != 0 && qol_invincible() && qol_ram(SHIP_BATTLE_DATA) && phase >= 0 && phase < 8) {
    int *damage = (int *)(SHIP_BATTLE_DATA + phase * 0x90 + 0x130);
    const int saved = *damage;
    *damage = 0;
    fn_8014419C(phase, side);
    *damage = saved;
    return;
  }
  fn_8014419C(phase, side);
}

TRK_CAVE static int qol_fast(void) {
  if (!qol_on() || QOL_SPEED < 2)
    return 0;
  return qol_in_battle() || ((qol_input.battle_speed & QOL_SPEED_FIELD) && GAME_MODE == 6);
}

/* Once a frame, from ws_frame_end just before fn_801DC420 (end of frame): out of battle the
 * per-battle marks are cleared; in battle Invincible refills; at speed the wait loop's mark is moved
 * back so the loop ends at once. */
TRK_CAVE void qol_damage(unsigned char *action, int target);
void dcm_frame(void); /* dcmusic.c: the Dreamcast music option switched mid-song */

/* A save state made after a Continue from a card save in ws20-ws22 (auto-saves too) has the resident
 * files' table empty (autoload.c), and its first battle read 0x00000004. In the field, load them the
 * way the second logo does: a step a frame (the reads finish on another thread, so a loop here never
 * ends), with no random encounters until they're in (qol_encounter_rand). Only the loader writes the
 * table, so a wrong sum means it never ran. */
TRK_CAVE static void qol_resident(void) {
  if (RESIDENT_LOAD == 5) {
    u32 sum = 0;
    for (int i = 0; i < 0x204; i++)
      sum += RESIDENT_TABLE[i];
    if (sum == RESIDENT_SUM)
      return;
    RESIDENT_LOAD = 0;
  }
  fn_8022A854(&RESIDENT_LOAD);
}

TRK_CAVE void qol_frame(void) {
  if (GAME_MODE == 6)
    qol_resident();
  if (ACTION_HANDLERS[0] != qol_damage)
    ACTION_HANDLERS[0] = qol_damage;
  if (GAME_MODE != 7)
    qol_scaled_ground = 0;
  /* A ship battle that ends without its results (game over, reset) leaves the flag set. */
  if (GAME_MODE == 6 && (MAP_ID < 500 || MAP_ID > 599))
    qol_ship_live = 0;
  if (qol_invincible())
    qol_refill();
  if (qol_fast())
    FRAME_START_RETRACE = VIGetRetraceCount() - 2;
  dcm_frame();
}

/* 0x801DC4D8 (qol_wait_retrace in qol.S, which tail-calls VIWaitForRetrace so the game never waits
 * inside patch code): at speed n, n frames take two retraces (n = 2: every frame waits for one).
 * Returns nonzero to skip this frame's wait. */
TRK_CAVE int qol_skip_wait(void) {
  if (!qol_fast()) {
    qol_speed_phase = 0;
    return 0;
  }
  qol_speed_phase += 2;
  if (qol_speed_phase < QOL_SPEED)
    return 1;
  qol_speed_phase -= QOL_SPEED;
  return 0;
}
