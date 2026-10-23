# Sections of their own (in a COMDAT group, or named like .text.foo) that
# start with their only global symbol become COMDAT records named by it, so
# the linker merges copies and drops unreferenced ones. Group members get
# "pick any"; a group section with no global symbol is named after the group.
# References go through an EXTDEF of the COMDAT name.
# RUN: llvm-mc -triple=i8086-pc-msdos -filetype=obj %s -o %t.obj
# RUN: %python %S/Inputs/omfdump.py %t.obj | FileCheck %s
# RUN: llvm-nm %t.obj | FileCheck %s --check-prefix=NM

# CHECK:      SEGDEF _TEXT {{.*}} len=0004
# CHECK:      EXTDEF '_inl' type=0
# CHECK-NEXT: EXTDEF '_inl$rodata' type=0
# CHECK-NOT:  EXTDEF
# CHECK-NEXT: PUBDEF '_f' group=0 seg=1 off=0000
# CHECK-NEXT: LEDATA seg=1 off=0000 len=4 e80000c3
# CHECK-NEXT:   FIXUP @001 self offset16 frame=F5 target=T6:1 disp=None
# CHECK:      LNAMES {{.*}}='_inl' {{.*}}='_inl$rodata' {{.*}}='_split' {{.*}}='_var'
# CHECK-NEXT: COMDAT '_inl' flags=00 attr=10 align=1 group=0 seg=1 off=0000 len=5 b8000047c3
# CHECK-NEXT:   FIXUP @001 seg offset16 frame=F5 target=T6:2 disp=None
# CHECK-NEXT: COMDAT '_inl$rodata' flags=00 attr=10 align=2 group=1 seg=2 off=0000 len=2 0400
# CHECK-NEXT:   FIXUP @000 seg offset16 frame=F5 target=T6:1 disp=None
# CHECK-NEXT: COMDAT '_split' flags=00 attr=00 align=1 group=0 seg=1 off=0000 len=2 40c3
# CHECK-NEXT: COMDAT '_var' flags=00 attr=00 align=2 group=1 seg=4 off=0000 len=2 0000
# CHECK-NEXT: MODEND

# NM-NOT: U
# NM:     T _f
# NM-NEXT: T _inl
# NM-NEXT: R _inl$rodata
# NM-NEXT: T _split
# NM-NEXT: B _var

	.text
	.globl	_f
_f:
	callw	_inl
	retw

	.section	.text._inl,"axG",@progbits,_inl,comdat
	.globl	_inl
_inl:
	movw	$.Ltab, %ax
	incw	%di
.Lcase:
	retw

	.section	.rodata._inl,"aG",@progbits,_inl,comdat
	.p2align	1
.Ltab:
	.short	.Lcase

	.section	.text._split,"ax",@progbits
	.globl	_split
_split:
	incw	%ax
	retw

	.section	.bss._var,"aw",@nobits
	.globl	_var
	.p2align	1
_var:
	.zero	2
