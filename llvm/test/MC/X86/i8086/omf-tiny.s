# Tiny memory model: _TEXT is in DGROUP, so every fixup uses DGROUP as its
# frame and the start address is DGROUP-relative.
# RUN: llvm-mc -triple=i8086-pc-msdos -target-abi=tiny -filetype=obj %s -o %t.obj
# RUN: %python %S/Inputs/omfdump.py %t.obj | FileCheck %s

# CHECK:      THEADR 'noname'
# CHECK-NEXT: COMENT class=00 b'\x04LLVM'
# CHECK-NEXT: LNAMES 1='' 2='DGROUP' 3='_TEXT' 4='CODE' 5='CONST' 6='DATA' 7='_DATA' 8='_BSS' 9='BSS'
# CHECK-NEXT: SEGDEF _TEXT align=byte combine=public big=0 use32=0 len=0007 class=CODE
# CHECK-NEXT: SEGDEF CONST align=byte combine=public big=0 use32=0 len=0000 class=DATA
# CHECK-NEXT: SEGDEF _DATA align=byte combine=public big=0 use32=0 len=0003 class=DATA
# CHECK-NEXT: SEGDEF _BSS align=byte combine=public big=0 use32=0 len=0000 class=BSS
# CHECK-NEXT: GRPDEF DGROUP segs=[1, 2, 3, 4]
# CHECK-NEXT: EXTDEF '_ext' type=0
# CHECK-NEXT: PUBDEF '_start' group=1 seg=1 off=0000
# CHECK-NEXT: LEDATA seg=1 off=0000 len=7 ba0000e80000c3
# CHECK-NEXT:   FIXUP @001 seg offset16 frame=F1:1 target=T4:3 disp=None
# CHECK-NEXT:   FIXUP @004 self offset16 frame=F1:1 target=T6:1 disp=None
# CHECK-NEXT: LEDATA seg=3 off=0000 len=3 686900
# CHECK-NEXT: MODEND main=1 start frame=F1:1 target=T0:1 disp=0000

	.text
	.globl	_start
_start:
	movw	$msg, %dx
	call	_ext
	ret
	.data
msg:
	.asciz	"hi"
