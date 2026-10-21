/* The European release's 50 Hz cutscene timing, played at 60 Hz (issue #3).
 *
 * Built into the DOL with widescreen.c by tools/gamepatch.py (spec: widescreen.json). The European
 * game has no 50 Hz compensation in code (it runs 5/6 speed at 50 Hz), but three of its scripts were
 * retimed for it: the scenes set to a streamed song, me299a (the intro, m01) and me298a / me250a
 * (m03). Every wait and motion time there is "x / 1.2" (the expression words 0x04000000 x,
 * 0x04000000 1.2f, 0x13 divide), where the USA and Japanese scripts have x. The songs are the same
 * files on every disc, so at the port's 60 Hz the European picture runs 1.2 times as fast as the
 * music. No other script, in any release, has that expression.
 *
 * The script loader fn_801F7158 copies a script's code into its buffer with one memcpy; pal_script_copy
 * does that copy, then makes each 1.2 there 1.0: the script keeps its size and layout, and x / 1.0 is
 * the USA's x. On the USA and Japanese discs there is nothing to change. */

typedef unsigned int u32;

#define TRK_CAVE __attribute__((section(".trkcave")))

extern void *memcpy(void *dst, const void *src, unsigned long n);

TRK_CAVE void *pal_script_copy(void *dst, const void *src, unsigned long n) {
  u32 *word = (u32 *)dst;
  u32 *const end = word + n / 4;
  memcpy(dst, src, n);
  for (; word + 3 <= end; word++)
    if (word[1] == 0x3F99999A && word[0] == 0x04000000 && word[2] == 0x13)
      word[1] = 0x3F800000; /* 1.0f */
  return dst;
}
