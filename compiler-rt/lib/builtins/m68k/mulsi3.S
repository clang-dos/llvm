//===-- mulsi3.S - 32-bit multiply for the 68000 --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// int __mulsi3(int a, int b): the low 32 bits of a * b, for CPUs before the
// 68020, which only multiply 16-bit values. The backend calls it for every
// 32-bit multiply there.
//
//===----------------------------------------------------------------------===//

// The assembly.h macros put several directives on a line with ";", which
// starts a comment in m68k assembly, so the directives are spelled out.

	.text
	.p2align 1
	.globl __mulsi3
	.type __mulsi3,@function
#ifdef VISIBILITY_HIDDEN
	.hidden __mulsi3
#endif
__mulsi3:
	move.l %d2, -(%sp)
	move.l (8,%sp), %d0	// a
	move.l (12,%sp), %d1	// b
	move.l %d0, %d2
	swap %d2
	mulu %d1, %d2		// high half of a * low half of b
	swap %d1
	mulu %d0, %d1		// low half of a * high half of b
	add.w %d1, %d2
	swap %d2		// the cross products go in the high half
	clr.w %d2
	move.w (14,%sp), %d1
	mulu %d1, %d0		// low half of a * low half of b
	add.l %d2, %d0
	move.l (%sp)+, %d2
	rts
	.size __mulsi3, .-__mulsi3

	.section .note.GNU-stack,"",@progbits
