# Small memory model OMF object: segments, DGROUP, publics, externals,
# communal variables and fixups, including base16 (sym@SEG) fixups.
# RUN: llvm-mc -triple=i8086-pc-msdos -filetype=obj %s -o %t.obj
# RUN: %python %S/Inputs/omfdump.py %t.obj | FileCheck %s

# CHECK:      THEADR 'omf-small.c'
# CHECK-NEXT: COMENT class=00 b'\x04LLVM'
# CHECK-NEXT: LNAMES 1='' 2='DGROUP' 3='_TEXT' 4='CODE' 5='CONST' 6='DATA' 7='_DATA' 8='_BSS' 9='BSS'
# CHECK-NEXT: SEGDEF _TEXT align=byte combine=public big=0 use32=0 len=0026 class=CODE
# CHECK-NEXT: SEGDEF CONST align=byte combine=public big=0 use32=0 len=0007 class=DATA
# CHECK-NEXT: SEGDEF _DATA align=word combine=public big=0 use32=0 len=0002 class=DATA
# CHECK-NEXT: SEGDEF _BSS align=word combine=public big=0 use32=0 len=0004 class=BSS
# CHECK-NEXT: GRPDEF DGROUP segs=[2, 3, 4]
# CHECK-NEXT: COMDEF '_buf' near size=300
# CHECK-NEXT: EXTDEF '_ext_data' type=0
# CHECK-NEXT: EXTDEF '_ext_func' type=0
# CHECK-NEXT: PUBDEF '_start' group=0 seg=1 off=0000
# CHECK-NEXT: PUBDEF '_main' group=0 seg=1 off=001F
# CHECK-NEXT: PUBDEF '_counter' group=1 seg=3 off=0000
# CHECK-NEXT: PUBDEF '_abs' group=0 seg=0 off=1234
# CHECK-NEXT: LEDATA seg=1 off=0000 len=38 e81f00b800008b1e0000b90400e80000eb00b80000b80000ba0000be0000c3e81200b80300c3
# CHECK-NEXT:   FIXUP @001 self offset16 frame=F5 target=T4:1 disp=None
# CHECK-NEXT:   FIXUP @004 seg offset16 frame=F1:1 target=T4:3 disp=None
# CHECK-NEXT:   FIXUP @008 seg offset16 frame=F1:1 target=T4:3 disp=None
# CHECK-NEXT:   FIXUP @00B seg offset16 frame=F5 target=T6:2 disp=None
# CHECK-NEXT:   FIXUP @00E self offset16 frame=F5 target=T6:3 disp=None
# CHECK-NEXT:   FIXUP @013 seg base16 frame=F1:1 target=T4:3 disp=None
# CHECK-NEXT:   FIXUP @016 seg base16 frame=F5 target=T6:2 disp=None
# CHECK-NEXT:   FIXUP @019 seg offset16 frame=F1:1 target=T4:2 disp=None
# CHECK-NEXT:   FIXUP @01C seg offset16 frame=F5 target=T6:1 disp=None
# CHECK-NEXT:   FIXUP @020 self offset16 frame=F5 target=T4:1 disp=None
# CHECK-NEXT:   FIXUP @023 seg offset16 frame=F1:1 target=T4:2 disp=None
# CHECK-NEXT: LEDATA seg=2 off=0000 len=7 6869001f001300
# CHECK-NEXT:   FIXUP @003 seg offset16 frame=F5 target=T4:1 disp=None
# CHECK-NEXT:   FIXUP @005 seg offset16 frame=F5 target=T4:1 disp=None
# CHECK-NEXT: LEDATA seg=3 off=0000 len=2 0100
# CHECK-NEXT: MODEND main=1 start frame=F0:1 target=T0:1 disp=0000

	.file	"omf-small.c"
	.text
	.globl	_start
_start:
	call	_main
	movw	$_counter, %ax
	movw	_counter, %bx
	movw	$_ext_data+4, %cx
	call	_ext_func
	jmp	local
local:
	movw	$_counter@SEG, %ax
	movw	$_ext_data@SEG, %ax
	movw	$msg, %dx
	movw	$_buf, %si
	ret

	.section .text.second,"ax",@progbits
	.globl	_main
_main:
	call	local
	movw	$table, %ax
	ret

	.section .rodata,"a",@progbits
msg:
	.asciz	"hi"
table:
	.short	_main
	.short	local+1

	.data
	.p2align 1
	.globl	_counter
_counter:
	.short	1

	.bss
	.p2align 1
_zero:
	.zero	4

	.comm	_buf,300,2
	.set	_abs, 0x1234
	.globl	_abs
