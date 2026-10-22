/* Debug aid, not for play builds: gives the party a ship, or a member, on request, so a test can start a ship
 * battle from a savestate made before the game hands out the Little Jack (the ship battle copies
 * the party's ship from the save, lbl_8030B7F4 + 0x1568; without one it has 0 HP). A test writes
 * 'SHP' << 8 | ship to SHIP_REQUEST (0 = Little Jack, 2 = Delphinus; table lbl_802D6740); the next
 * field frame calls the game's own ship setup (fn_8021BD7C) and clears the word. Then a map change
 * to a ship battle map (500-599: write "me500a.sct" to lbl_80305CF0 and 0xF to the field state
 * 0x80311AEC, as fn_80100270 does) starts the battle. Build it together with widescreen.json.
 * A member: 'PTY' << 8 | member (0-5; 3 = Drachma) to PARTY_REQUEST joins them through the game's
 * own join (fn_800FF6B4, which a script's join runs: stats from their table on a first join).
 * dbg_field (shipdebug.S) tail-calls the field main, so no frame of this code is on the stack when
 * a savestate is made: states made with this build load in play builds. */

typedef unsigned int u32;

#pragma clang section text=".trkcave"

extern void fn_8021BD7C(int ship);  /* set the party's ship (-1 = none) */
extern void fn_800FF6B4(int member); /* a member joins the party */

/* The last word of MetroTRK's message buffers (lbl_80319078, 0x19B0 bytes, unused without a
 * debugger; see cave.ld): past trace2d.c's table (0x1810 bytes from the start), so the two debug
 * patches can be built together. */
#define SHIP_REQUEST (*(volatile u32 *)0x8031AA24)
#define SHIP_MAGIC 0x534850u /* 'SHP' */
#define PARTY_REQUEST (*(volatile u32 *)0x8031AA20)
#define PARTY_MAGIC 0x505459u /* 'PTY' */

void dbg_ship_request(void) {
  u32 request = SHIP_REQUEST;
  if ((request >> 8) == SHIP_MAGIC) {
    SHIP_REQUEST = 0;
    fn_8021BD7C((int)(signed char)(request & 0xFF));
  }
  request = PARTY_REQUEST;
  if ((request >> 8) == PARTY_MAGIC) {
    PARTY_REQUEST = 0;
    if ((request & 0xFF) < 6)
      fn_800FF6B4((int)(request & 0xFF));
  }
}
