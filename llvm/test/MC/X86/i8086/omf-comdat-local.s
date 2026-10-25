# A section of its own with no global symbol (a static function or variable,
# or a function's jump table) is a local COMDAT (flag 04), which the linker
# drops when nothing in the module uses it. It is named by its local symbol,
# or after the section, and referenced through an LEXTDEF. A global alias of
# a COMDAT's start (a C++ complete-object constructor) is an ALIAS record
# naming the COMDAT; an alias elsewhere keeps the section in the segment.
# RUN: llvm-mc -triple=i8086-pc-msdos -filetype=obj %s -o %t.obj
# RUN: %python %S/Inputs/omfdump.py %t.obj | FileCheck %s
# RUN: llvm-nm %t.obj | FileCheck %s --check-prefix=NM

# CHECK:      SEGDEF _TEXT {{.*}} len=0002
# CHECK:      LEXTDEF '_helper' type=0
# CHECK-NEXT: LEXTDEF '$.rodata._ctor_base$3' type=0
# CHECK-NEXT: EXTDEF '_ctor_base' type=0
# CHECK-NEXT: PUBDEF '_inside' group=0 seg=1 off=0000
# CHECK-NEXT: PUBDEF '_mid' group=0 seg=1 off=0001
# CHECK-NEXT: ALIAS '_ctor_complete' -> '_ctor_base'
# CHECK-NEXT: LEDATA seg=1 off=0000 len=2 40c3
# CHECK-NEXT: LNAMES {{.*}}='_ctor_base' {{.*}}='_helper' {{.*}}='_counter' {{.*}}='$.rodata._ctor_base$3'
# CHECK-NEXT: COMDAT '_ctor_base' flags=00 attr=00 align=1 group=0 seg=1 off=0000 len=7 e80000b80000c3
# CHECK-NEXT:   FIXUP @001 self offset16 frame=F5 target=T6:1 disp=None
# CHECK-NEXT:   FIXUP @004 seg offset16 frame=F5 target=T6:2 disp=None
# CHECK-NEXT: COMDAT '_helper' flags=04 attr=00 align=1 group=0 seg=1 off=0000 len=2 40c3
# CHECK-NEXT: COMDAT '_counter' flags=04 attr=00 align=2 group=1 seg=4 off=0000 len=2 0000
# CHECK-NEXT: COMDAT '$.rodata._ctor_base$3' flags=04 attr=00 align=2 group=1 seg=2 off=0000 len=2 0600
# CHECK-NEXT:   FIXUP @000 seg offset16 frame=F5 target=T6:3 disp=None
# CHECK-NEXT: MODEND

# NM-NOT:  U
# NM:      r $.rodata._ctor_base$3
# NM-NEXT: b _counter
# NM-NEXT: T _ctor_base
# NM-NEXT: T _ctor_complete
# NM-NEXT: t _helper
# NM-NEXT: T _inside
# NM-NEXT: T _mid

	.section	.text._ctor_base,"ax",@progbits
	.globl	_ctor_base
_ctor_base:
	callw	_helper
	movw	$.Ltab, %ax
.Lcase:
	retw

	.globl	_ctor_complete
	.set	_ctor_complete, _ctor_base

	.section	.text._helper,"ax",@progbits
_helper:
	incw	%ax
	retw

	.section	.bss._counter,"aw",@nobits
	.p2align	1
_counter:
	.zero	2

	.section	.rodata._ctor_base,"a",@progbits
	.p2align	1
.Ltab:
	.short	.Lcase

	.section	.text._inside,"ax",@progbits
	.globl	_inside
_inside:
	incw	%ax
	retw

	.globl	_mid
	.set	_mid, _inside+1
