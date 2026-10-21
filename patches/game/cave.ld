/* Layout of the patch code: the low-memory gap between the exception vectors and the DOL
 * (0x80001800-0x80003000, where Gecko's code handler would live). Dolphin uses a few words of it
 * even with cheats off (Core/HLE/HLE.cpp: a stub at 0x80001800-0x8000180C, an HLE hook at
 * 0x800018A8, a trampoline at 0x80002FFC), so the patch starts at 0x80001900 and ends at
 * 0x80002F00. Enabling Gecko codes in Dolphin installs the code handler over this range.
 * tools/gamepatch.py adds .text (code and constants) as a DOL text section and .data (config,
 * state, zeroed bss) as a DOL data section. */
SECTIONS
{
  . = 0x80001900;
  /* Constants go with the code: a savestate made with another build carries that build's data
   * section, but recompcore-restore-boot-dol.patch puts this build's code sections back. */
  .text : { *(.text .text.*) *(.rodata .rodata.* .sdata2 .sdata2.*) }
  ASSERT(. <= 0x80002A00, "patch code doesn't fit below 0x80002A00")
  . = 0x80002A00;
  .data : {
    KEEP(*(.ws_config))
    KEEP(*(.trace2d))
    *(.data .data.* .sdata .sdata.*)
    *(.bss .bss.* .sbss .sbss.* COMMON)
    /* Variables added to a later build (__attribute__((section(".late_bss")))), after all the
     * others, so a save state made with an earlier build still has every older variable where this
     * build looks for it, and 0 in the new ones. */
    KEEP(*(.late_bss))
    /* Then each later build's own, for the same reason (ws28: qol_normal_speed, which the frontend
     * reads at its address). */
    KEEP(*(.late_bss_ws28))
    /* widescreen.c's HUD width, then qol.c's and camera.c's input blocks, at fixed addresses
     * the frontend writes (Core/SkiesSettings.cpp, VideoCommon/SkiesMenu.cpp). */
    ASSERT(. <= 0x80002E9C, "patch data runs into ws_hud_aspect at 0x80002E9C")
    . = 0x80002E9C;
    KEEP(*(.ws_hud))
    . = 0x80002EA0;
    KEEP(*(.qol_input))
    . = 0x80002EC0;
    KEEP(*(.cam_input))
    /* autoload.c's load_request, which the frontend writes at boot. */
    . = 0x80002EF0;
    KEEP(*(.load_request))
  }
  ASSERT(. <= 0x80002F00, "patch data doesn't fit below 0x80002F00")
  /* A second code area, in place of MetroTRK's message handlers (fn_802548E8-fn_80256FC4, 10 KB):
   * they're reached only through MetroTRK's dispatch table (lbl_802FBCF0, read by
   * TRKDispatchMessage), and MetroTRK only starts with a debugger (__start checks 0x800030E8 for 2
   * or 3; retail and Dolphin have 0). Since ws29 it goes on over the next MetroTRK functions
   * (fn_80257074-fn_80257A20, 2.6 KB), which only InitMetroTRK reaches (nothing outside MetroTRK
   * calls or points at them); it stops at TRK_flush_cache (0x80257AF8), which the game calls. Put code here with section ".trkcave" (e.g. a file-wide
   * #pragma clang section text=".trkcave"); tools/gamepatch.py writes it over the DOL's text in
   * place. It's code only: its data and constants still go above. Last, so the ELF file stays
   * in address order. */
  .trkcave 0x802548E8 : { *(.trkcave .trkcave.*) }
  ASSERT(. <= 0x80257AF8, "patch code doesn't fit in the MetroTRK area (0x802548E8-0x80257AF8)")
  /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}
