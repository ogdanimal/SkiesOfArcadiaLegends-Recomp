/* Field areas: backdrop scenery kept where a map's own lists drop it (issue #21).
 *
 * A field map's objects (the map file's table: map[0] entries of 0x68 bytes at map[1], +0 index,
 * +4 id, +0xC the list, +0x34 the stamp) are only drawn in the areas that list them: when the party
 * steps into another area (fn_800D2CC4, from the floor's area id), the area state (lbl_80305BE8)
 * counts up (+0x3C) and fn_800D29B4 writes the count into every object the new area's list names;
 * fn_800D2924 hides an object whose stamp isn't the current count.
 *
 * Sailors' Island (me004a.sct, a004a.mld): the bridge to the lighthouse is its own area (id 901,
 * "hasi"), and its list leaves out the sky's backdrop, objects 2504-2506 (the two clouds of small
 * floating rocks and the broken tower with its rocks), which every other outdoor area of the map
 * (ids 0-5 and 208) lists. The bridge's camera looks straight at them, so they vanished on the
 * first step onto the bridge and came back on the last. fn_800D2CC4's call for the new area's list
 * (0x800D2E0C) comes here, and on that bridge those three get the stamp too. Widescreen only: 4:3
 * stays the game's own picture. */

typedef unsigned int u32;

#define MAP_ID (*(volatile int *)0x80311AC4)
#define MAP_LETTER (((volatile unsigned char *)0x80311AC4)[4])
/* lbl_80305BE8 +0x30: the current area's entry in the map's table */
#define AREA_ENTRY (*(u32 *volatile *)(0x80305BE8 + 0x30))

extern void fn_800D29B4(u32 stamp, u32 *map, const int *list);
extern float ws_factor(void);

__attribute__((section(".trkcave_ws36"))) void ws_area_list(u32 stamp, u32 *map, const int *list) {
  float factor = ws_factor();
  u32 *entry = AREA_ENTRY;
  u32 i;

  fn_800D29B4(stamp, map, list);
  if (factor <= 0.0f || factor >= 1.0f || MAP_ID != 4 || MAP_LETTER != 'a' || map == 0 || entry == 0 ||
      entry[1] != 901)
    return;
  entry = (u32 *)map[1];
  for (i = 0; i < map[0]; i++, entry += 0x68 / 4)
    if (entry[1] >= 2504 && entry[1] <= 2506)
      entry[0x34 / 4] = stamp;
}
