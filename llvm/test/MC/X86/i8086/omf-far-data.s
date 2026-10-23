# Far data: sections .fardata.<sym>, .farbss.<sym> and .farrodata.<sym> of
# 4 KiB or more become private segments <sym>_DATA, <sym>_BSS and
# <sym>_CONST of classes FAR_DATA and FAR_BSS, outside DGROUP and after the
# code. Smaller ones share the segments FAR_DATA, FAR_CONST and FAR_BSS. A
# section over 64 KiB is split into paragraph aligned 64 KiB parts. Offsets
# past 64 KiB move to a later part, and so does each 0x1000 of a segment
# addend. A far section in a group is a COMDAT in the shared segment.
# RUN: llvm-mc -triple=i8086-pc-msdos -filetype=obj %s -o %t.obj
# RUN: %python %S/Inputs/omfdump.py %t.obj | FileCheck %s

# CHECK:      LNAMES 1='' 2='DGROUP' 3='_TEXT' 4='CODE' 5='FAR_DATA' 6='zero_BSS' 7='FAR_BSS' 8='FAR_CONST' 9='big_BSS' 10='CONST' 11='DATA' 12='_DATA' 13='_BSS' 14='BSS'
# CHECK-NEXT: SEGDEF _TEXT align=byte combine=public big=0 use32=0 len=0018 class=CODE
# CHECK-NEXT: SEGDEF FAR_DATA align=word combine=private big=0 use32=0 len=0007 class=FAR_DATA
# CHECK-NEXT: SEGDEF zero_BSS align=word combine=private big=0 use32=0 len=8000 class=FAR_BSS
# CHECK-NEXT: SEGDEF FAR_CONST align=byte combine=private big=0 use32=0 len=0003 class=FAR_DATA
# CHECK-NEXT: SEGDEF big_BSS align=para combine=private big=1 use32=0 len=0000 class=FAR_BSS
# CHECK-NEXT: SEGDEF big_BSS align=para combine=private big=1 use32=0 len=0000 class=FAR_BSS
# CHECK-NEXT: SEGDEF big_BSS align=para combine=private big=0 use32=0 len=0010 class=FAR_BSS
# CHECK-NEXT: SEGDEF CONST align=byte combine=public big=0 use32=0 len=0000 class=DATA
# CHECK-NEXT: SEGDEF _DATA align=byte combine=public big=0 use32=0 len=0008 class=DATA
# CHECK-NEXT: SEGDEF _BSS align=byte combine=public big=0 use32=0 len=0000 class=BSS
# CHECK-NEXT: GRPDEF DGROUP segs=[8, 9, 10]
# CHECK-NEXT: EXTDEF '_ZTV1A' type=0
# CHECK-NEXT: PUBDEF 'ptrs' group=1 seg=9 off=0000
# CHECK-NEXT: PUBDEF 'zero' group=0 seg=3 off=0000
# CHECK-NEXT: PUBDEF 'big' group=0 seg=5 off=0000
# CHECK-NEXT: LEDATA seg=1 off=0000 len=24 b80000b80000b80000b81000a10000a12000b80000b80600
# CHECK-NEXT:   FIXUP @001 seg base16 frame=F5 target=T4:2 disp=None
# CHECK-NEXT:   FIXUP @004 seg base16 frame=F5 target=T4:5 disp=None
# CHECK-NEXT:   FIXUP @007 seg base16 frame=F5 target=T4:6 disp=None
# CHECK-NEXT:   FIXUP @00A seg base16 frame=F5 target=T4:7 disp=None
# CHECK-NEXT:   FIXUP @00D seg offset16 frame=F5 target=T4:3 disp=None
# CHECK-NEXT:   FIXUP @010 seg offset16 frame=F5 target=T4:6 disp=None
# CHECK-NEXT:   FIXUP @013 seg base16 frame=F5 target=T6:1 disp=None
# CHECK-NEXT:   FIXUP @016 seg offset16 frame=F5 target=T4:2 disp=None
# CHECK-NEXT: LEDATA seg=2 off=0000 len=7 01000200030009
# CHECK-NEXT: LEDATA seg=4 off=0000 len=3 726f00
# CHECK-NEXT: LEDATA seg=9 off=0000 len=8 0400000020000000
# CHECK-NEXT:   FIXUP @000 seg ptr16:16 frame=F5 target=T4:2 disp=None
# CHECK-NEXT:   FIXUP @004 seg ptr16:16 frame=F5 target=T4:6 disp=None
# CHECK-NEXT: LNAMES 15='_ZTV1A'
# CHECK-NEXT: COMDAT '_ZTV1A' flags=00 attr=10 align=2 group=0 seg=4 off=0000 len=4 01000200

	.text
	movw	$init@SEG, %ax
	movw	$big@SEG, %ax
	movw	$big@SEG+0x1000, %ax
	movw	$big@SEG+0x2010, %ax
	movw	zero, %ax
	movw	big+0x10020, %ax
	movw	$_ZTV1A@SEG, %ax
	movw	$init2, %ax

	.section .fardata.init,"aw",@progbits
	.p2align 1
init:
	.short	1, 2, 3

	.section .fardata.init2,"aw",@progbits
init2:
	.byte	9

	.section .farrodata._ZTV1A,"aG",@progbits,_ZTV1A,comdat
	.weak	_ZTV1A
	.p2align 1
_ZTV1A:
	.short	1, 2

	.section .farbss.zero,"aw",@nobits
	.globl	zero
	.p2align 1
zero:
	.zero	0x8000

	.section .farrodata.ro,"a",@progbits
ro:
	.asciz	"ro"

	.section .farbss.big,"aw",@nobits
	.globl	big
big:
	.zero	0x20010

	.data
	.globl	ptrs
ptrs:
	.long	init+4
	.long	big+0x10020
