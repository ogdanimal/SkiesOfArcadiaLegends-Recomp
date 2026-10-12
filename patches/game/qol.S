/* qol.c's battle speed: the end-of-frame retrace wait (fn_801DC420, 0x801DC4D8). Tail-calls
 * VIWaitForRetrace so the game never waits inside patch code (a savestate made then would return
 * into this build's code). */
	.section .trkcave, "ax"
	.global qol_wait_retrace
	.type qol_wait_retrace, @function
qol_wait_retrace:
	stwu 1, -16(1)
	mflr 0
	stw 0, 20(1)
	bl qol_skip_wait
	lwz 0, 20(1)
	mtlr 0
	addi 1, 1, 16
	cmpwi 3, 0
	bnelr
	b VIWaitForRetrace
	.size qol_wait_retrace, . - qol_wait_retrace
