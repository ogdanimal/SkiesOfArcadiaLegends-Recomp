/* Full-width wrappers for widescreen.c: call a 2D function with the orthographic projection's x
 * scale (lbl_803463C0 [0][0]) at the full screen width, then restore what was there. Used for
 * the Ninja screen-copy and fade effects and the script fade, which would otherwise be squeezed
 * into the HUD's 4:3 area. Only r0, r11 and r12 are touched, so the wrapped function's
 * arguments and return values pass through; the saved value makes nesting safe. Not for functions
 * that take arguments on the stack (none of the wrapped ones do). */

/* ws_ortho_m00 is the ortho matrix's [0][0] (lbl_803463C0 in the USA game), defined by the spec's
 * "symbols" so gamepatch translates it per game version (literal address halves here were not
 * translated, and the European build wrote the full-width scale to the wrong word). */

/* In the original 4:3 picture (ws_config.factor >= 1, which ws_apply sets for display_aspect 0)
 * every wrapper here is a no-op, but its own stack frame isn't: with the text wrappers' frames in
 * place, 4:3 differed from the unpatched game by faint pixels on most edges in every battle frame
 * (Windows dumps, 2026-10-10), gone without them. So in 4:3 each wrapper jumps straight to the
 * game's function, with no frame, and the game runs its original path. Touches r11, r12 and cr0
 * only (volatile; the wrapped functions take their arguments in r3-r10 and f1-f8). The factor is a
 * positive float, so an integer compare with 1.0f's bits works. */
#define WS_CONFIG_FACTOR 20 /* offsetof(struct ws_config, factor) */
.macro ORIGINAL_IN_43 target
	lis 12, ws_config@ha
	addi 12, 12, ws_config@l
	lwz 12, WS_CONFIG_FACTOR(12)
	lis 11, 0x3F80 /* 1.0f */
	cmpw 12, 11
	blt 1f
	b \target
1:
.endm

/* A hook written in C (the _wide function) that does nothing in 4:3. */
.macro GATE name, target, wide
	.global \name
	.type \name, @function
\name:
	ORIGINAL_IN_43 \target
	b \wide
	.size \name, . - \name
.endm

.macro FULLWIDTH name, target
	.global \name
	.type \name, @function
\name:
	ORIGINAL_IN_43 \target
	stwu 1, -16(1)
	mflr 0
	stw 0, 20(1)
	lis 11, ws_ortho_m00@ha
	lwz 12, ws_ortho_m00@l(11)
	stw 12, 8(1)
	lis 12, ws_config@ha
	addi 12, 12, ws_config@l
	lwz 12, 12(12) /* ws_config.full_m00 */
	stw 12, ws_ortho_m00@l(11)
	bl \target
	lwz 12, 8(1)
	lis 11, ws_ortho_m00@ha
	stw 12, ws_ortho_m00@l(11)
	lwz 0, 20(1)
	mtlr 0
	addi 1, 1, 16
	blr
	.size \name, . - \name
.endm

	.text
/* Ninja screen copy and fade effects (called from fn_801D1714 every frame). */
FULLWIDTH ws_full_802AB460, fn_802AB460
FULLWIDTH ws_full_802AB608, fn_802AB608
/* Battle-entry transitions: the task fn_800B0FD0 copies the screen and draws it back with 2D
 * polygons, as tiles turning away (fn_800B0278, kind 0) or through fn_800AF780 / fn_800AF8F8
 * (kind 1). The tiles don't reach both screen edges one by one, so ws_tex_poly can't tell. */
FULLWIDTH ws_full_800AF8F8, fn_800AF8F8
FULLWIDTH ws_full_800AF780, fn_800AF780
FULLWIDTH ws_full_800B0278, fn_800B0278
/* ScreenFadeDraw: the script fade, a 640 x 480 colour quad drawn through Ninja's own primitive
 * (fn_8028A0F4, which ws_poly doesn't see). Its rectangle comes in f2-f5, which pass through. */
FULLWIDTH ws_full_80210390, fn_80210390

/* Unwidened 3D (widescreen.c ws_unwide_begin / ws_unwide_end): call a 3D draw with the game's own
 * field of view. The argument registers (r3-r10, f1-f8) are kept across ws_unwide_begin and the
 * return values (r3, r4, f1) across ws_unwide_end. Not for functions that take arguments on the
 * stack. */
.macro UNWIDE name, target
	.global \name
	.type \name, @function
\name:
	ORIGINAL_IN_43 \target
	stwu 1, -112(1)
	mflr 0
	stw 0, 116(1)
	stw 3, 8(1)
	stw 4, 12(1)
	stw 5, 16(1)
	stw 6, 20(1)
	stw 7, 24(1)
	stw 8, 28(1)
	stw 9, 32(1)
	stw 10, 36(1)
	stfd 1, 40(1)
	stfd 2, 48(1)
	stfd 3, 56(1)
	stfd 4, 64(1)
	stfd 5, 72(1)
	stfd 6, 80(1)
	stfd 7, 88(1)
	stfd 8, 96(1)
	bl ws_unwide_begin
	lwz 3, 8(1)
	lwz 4, 12(1)
	lwz 5, 16(1)
	lwz 6, 20(1)
	lwz 7, 24(1)
	lwz 8, 28(1)
	lwz 9, 32(1)
	lwz 10, 36(1)
	lfd 1, 40(1)
	lfd 2, 48(1)
	lfd 3, 56(1)
	lfd 4, 64(1)
	lfd 5, 72(1)
	lfd 6, 80(1)
	lfd 7, 88(1)
	lfd 8, 96(1)
	bl \target
	stw 3, 8(1)
	stw 4, 12(1)
	stfd 1, 40(1)
	bl ws_unwide_end
	lwz 3, 8(1)
	lwz 4, 12(1)
	lfd 1, 40(1)
	lwz 0, 116(1)
	mtlr 0
	addi 1, 1, 112
	blr
	.size \name, . - \name
.endm

/* The battle-entry shatter: fn_802297C4 (a screen-effect task) draws the copied screen as 3D tiles
 * through fn_80287B38 (found with trace2d: the only projection chain new in the transition). */
UNWIDE ws_unwide_80287B38, fn_80287B38
/* The other battle-entry shatter: fn_80101828 copies the screen (fn_80226D14) and breaks it into
 * falling 3D triangles drawn by fn_80097CA4 (-> fn_80097B0C -> fn_80097CEC -> fn_80098DA4, each
 * called only on this path). */
UNWIDE ws_unwide_80097CA4, fn_80097CA4

/* Text anchoring (widescreen.c): call a text function, then pass the node it returns (r3)
 * through ws_text_anchor, which moves it if anchored HUD code queued it. Only for functions with
 * all their arguments in registers. */
.macro TEXTANCHOR name, target
	.global \name
	.type \name, @function
\name:
	ORIGINAL_IN_43 \target
	stwu 1, -16(1)
	mflr 0
	stw 0, 20(1)
	bl \target
	bl ws_text_anchor
	lwz 0, 20(1)
	mtlr 0
	addi 1, 1, 16
	blr
	.size \name, . - \name
.endm

TEXTANCHOR ws_text_print, TextPrint
TEXTANCHOR ws_text_print_n, TextPrintN
TEXTANCHOR ws_text_print_number, TextPrintNumber
/* Variadic, but no caller passes arguments on the stack (all 39 calls checked, 2026-10-11), so the
 * wrapper's frame doesn't shift any. */
TEXTANCHOR ws_text_printf, TextPrintf

/* The C hooks (widescreen.c), widescreen only. Not ws_set_projection: in 4:3 it puts the game's
 * scissor back after every projection load, which undoes the 4:3 clip that 16:9 left (a switch
 * from 16:9, or the first frames before the runtime's setting lands); bypassed, the HUD stayed cut
 * to the 16:9 HUD area. It has no visible cost in 4:3 (Windows dumps). */
GATE ws_text_draw_node, TextDrawNode, ws_text_draw_node_wide
GATE ws_text_flush, TextFlush, ws_text_flush_wide
GATE ws_project_hud, njProjectScreen, ws_project_hud_wide
GATE ws_poly, fn_80290ED4, ws_poly_wide
GATE ws_tex_poly, fn_80291064, ws_tex_poly_wide

/* Replaces main's call to fn_801DC420 (end of frame: copy to XFB, wait for retrace, flip): apply
 * the widescreen settings, then jump to it, so it returns straight to main. The game spends most
 * of its time waiting inside fn_801DC420, so that's where savestates catch it; with a tail call,
 * no return address into patch code is on the stack then, and states keep working when the patch
 * code moves in a later build. */
	.global ws_frame_end
	.type ws_frame_end, @function
ws_frame_end:
	stwu 1, -16(1)
	mflr 0
	stw 0, 20(1)
	bl ws_apply
	bl qol_frame
	bl cs_frame
	lwz 0, 20(1)
	mtlr 0
	addi 1, 1, 16
	b fn_801DC420
	.size ws_frame_end, . - ws_frame_end

/* Ship battle HP plates (fn_8015887C): note which plate is drawn, for ws_anchor_find (an enemy's
 * plate and a party ship's go to opposite edges), then jump on, so no patch frame is left. */
	.global ws_ship_plate_draw
	.type ws_ship_plate_draw, @function
ws_ship_plate_draw:
	lis 12, ws_ship_plate@ha
	stw 3, ws_ship_plate@l(12)
	b fn_8021C62C
	.size ws_ship_plate_draw, . - ws_ship_plate_draw

	.global ws_ship_plate_icon
	.type ws_ship_plate_icon, @function
ws_ship_plate_icon:
	lis 12, ws_ship_plate@ha
	stw 3, ws_ship_plate@l(12)
	b fn_80159610
	.size ws_ship_plate_icon, . - ws_ship_plate_icon

/* Ship battle popups (fn_8021F330, fn_8021F0EC): note the sprite (x, y first), then jump on. */
	.global ws_popup_draw
	.type ws_popup_draw, @function
ws_popup_draw:
	lis 12, ws_popup_sprite@ha
	stw 3, ws_popup_sprite@l(12)
	b fn_80005D98
	.size ws_popup_draw, . - ws_popup_draw
