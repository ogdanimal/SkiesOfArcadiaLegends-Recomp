/* shipdebug.c's hook on the field main (0x801DC358): handles a ship request, then tail-calls
 * fn_80101828 so no frame of patch code stays on the stack (savestates made in the field load in
 * builds without this code). */
	.section .trkcave, "ax"
	.global dbg_field
	.type dbg_field, @function
dbg_field:
	stwu 1, -16(1)
	mflr 0
	stw 0, 20(1)
	bl dbg_ship_request
	lwz 0, 20(1)
	mtlr 0
	addi 1, 1, 16
	b fn_80101828
	.size dbg_field, . - dbg_field
