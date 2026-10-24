# DGROUP@SEG is the segment of DGROUP itself: a group fixup, not an
# external symbol, as "mov ax, DGROUP" in MASM.
# RUN: llvm-mc -triple=i8086-pc-msdos -filetype=obj %s -o %t.obj
# RUN: %python %S/Inputs/omfdump.py %t.obj | FileCheck %s

# CHECK:     GRPDEF DGROUP
# CHECK-NOT: EXTDEF
# CHECK:     LEDATA seg=1 off=0000 len=5 b800008ed8
# CHECK-NEXT:  FIXUP @001 seg base16 frame=F1:1 target=T5:1 disp=None

	.text
	movw	$DGROUP@SEG, %ax
	movw	%ax, %ds
