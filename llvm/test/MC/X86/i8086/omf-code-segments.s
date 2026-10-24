# With far code, a module's code that does not fit in 64 KiB goes on in more
# code segments, <module>_TEXT_1 and so on, of class CODE. Each section,
# and room for each code COMDAT, goes in the current one while it fits.
# Far calls and far pointers reach any of them; a near jump between two of
# them is an error.
# RUN: llvm-mc -triple=i8086-pc-msdos -target-abi=large -filetype=obj %s -o %t.obj
# RUN: %python %S/Inputs/omfdump.py %t.obj | FileCheck %s
# RUN: not llvm-mc -triple=i8086-pc-msdos -target-abi=large -filetype=obj \
# RUN:   -defsym=ERR=1 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR
# RUN: not llvm-mc -triple=i8086-pc-msdos -target-abi=small -filetype=obj \
# RUN:   %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=NEAR

# CHECK:      SEGDEF big_TEXT align=byte combine=public big=0 use32=0 len=C005 class=CODE
# CHECK-NEXT: SEGDEF big_TEXT_1 align=byte combine=public big=0 use32=0 len=8005 class=CODE
# CHECK-NEXT: SEGDEF big_TEXT_2 align=byte combine=public big=0 use32=0 len=0000 class=CODE
# CHECK-NEXT: SEGDEF CONST
# CHECK:      PUBDEF '_a' group=0 seg=1 off=0000
# CHECK:      PUBDEF '_d' group=0 seg=2 off=8000
# CHECK:      LEDATA seg=1 off=0000
# CHECK-NEXT:   FIXUP @001 seg offset16 frame=F5 target=T4:2
# CHECK-NEXT:   FIXUP @003 seg base16 frame=F5 target=T4:2
# CHECK:      LEDATA seg=2 off=7F80
# CHECK-NEXT:   FIXUP @081 seg offset16 frame=F5 target=T6:1
# CHECK-NEXT:   FIXUP @083 seg base16 frame=F5 target=T6:1
# CHECK:      COMDAT '_c' flags=00 attr=10 align=1 group=0 seg=3 off=0000

# NEAR: error: segment _TEXT is larger than 64 KiB

	.file "big.c"
	# Two global symbols: not a COMDAT.
	.section .text._a,"ax",@progbits
	.globl _a, _a2
_a:
_a2:
	lcallw $_b@SEG, $_b
	.fill 0xc000, 1, 0x90

	# A local function.
	.section .text._b,"ax",@progbits
_b:
	lretw
	.fill 0x7fff, 1, 0x90

	# 0x9000 bytes do not fit in either segment.
	.section .text._c,"axG",@progbits,_c,comdat
	.weak _c
_c:
	.fill 0x9000, 1, 0x90

	.section .text._d,"ax",@progbits
	.globl _d, _d2
_d:
_d2:
	lcallw $_c@SEG, $_c
.ifdef ERR
# ERR: :[[#@LINE+1]]:{{[0-9]+}}: error: PC-relative reference to '_a' in another code segment of the module
	jmp _a
.endif
