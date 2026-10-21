/* cutscene.c's camera commands: one 16-byte stub per opcode in the script VM's handler table
 * (redirect_pointers, and cs_frame every frame), which loads the game's handler and goes to
 * cs_camera_command. A stub's first instruction is li r11, opcode: cs_frame reads the opcode from
 * it, so the opcodes are listed once, here. */
	.section .trkcave, "ax"

	.macro STUB op, handler
	.global cs_op_\op
	.type cs_op_\op, @function
cs_op_\op:
	li 11, 0x\op
	lis 12, \handler@ha
	addi 12, 12, \handler@l
	b cs_camera_command
	.size cs_op_\op, . - cs_op_\op
	.endm

	.global cs_stubs
cs_stubs:
	STUB 21, fn_80203FFC	/* camera along a vector path */
	STUB 2D, fn_8020726C	/* back to the saved camera pose */
	STUB 2E, fn_8020549C	/* turn the camera (Euler angles) */
	STUB 2F, fn_80204F8C	/* turn the camera's look-at */
	STUB 32, fn_80204CD0	/* camera pose */
	STUB 34, fn_80204BE8	/* camera position and look-at */
	STUB 79, fn_802034B0	/* camera angle path */
	STUB 7D, fn_8020731C	/* stop the camera's tasks */
	STUB 8B, fn_80202DEC	/* looping camera path */
	STUB 9F, fn_802023BC	/* eased camera path */
	.global cs_stubs_end
cs_stubs_end:

/* handler(flags r3, context r4) in r12: run it, then cs_note(context, the PC it started at), and
 * return its result. */
	.type cs_camera_command, @function
cs_camera_command:
	stwu 1, -32(1)
	mflr 0
	stw 0, 36(1)
	stw 31, 28(1)
	stw 30, 24(1)
	mr 31, 4
	lwz 30, 0x1C(4)
	mtctr 12
	bctrl
	stw 3, 20(1)
	mr 3, 31
	mr 4, 30
	bl cs_note
	lwz 3, 20(1)
	lwz 30, 24(1)
	lwz 31, 28(1)
	lwz 0, 36(1)
	mtlr 0
	addi 1, 1, 32
	blr
	.size cs_camera_command, . - cs_camera_command
