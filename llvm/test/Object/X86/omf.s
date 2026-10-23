# Read OMF objects with llvm-nm, llvm-objdump and llvm-ar.
# RUN: llvm-mc -triple=i8086-pc-msdos -filetype=obj %s -o %t.obj
# RUN: llvm-nm %t.obj | FileCheck %s --check-prefix=NM
# RUN: llvm-objdump -h -p -t %t.obj | FileCheck %s --check-prefix=HDR
# RUN: llvm-objdump -d -r %t.obj | FileCheck %s --check-prefix=DIS
# RUN: llvm-objdump -s -j CONST -j _DATA %t.obj | FileCheck %s --check-prefix=DATA
# RUN: rm -f %t.a
# RUN: llvm-ar rcs %t.a %t.obj
# RUN: llvm-nm --print-armap %t.a | FileCheck %s --check-prefix=ARMAP

# NM:      00001234 A _abs
# NM-NEXT: 0000012c C _buf
# NM-NEXT: 00000000 D _counter
# NM-NEXT:          U _ext_data
# NM-NEXT:          U _ext_func
# NM-NEXT: 00000016 T _main
# NM-NEXT: 00000000 T _start

# HDR:      file format omf-16
# HDR:      Module: omf.c
# HDR-NEXT: Segments:
# HDR-NEXT:   _TEXT class=CODE size=0x001a align=1 combine=2
# HDR-NEXT:   CONST class=DATA group=DGROUP size=0x0003 align=1 combine=2
# HDR-NEXT:   _DATA class=DATA group=DGROUP size=0x0002 align=2 combine=2
# HDR-NEXT:   _BSS class=BSS group=DGROUP size=0x0004 align=2 combine=2
# HDR:      Idx Name          Size     VMA  Type
# HDR-NEXT:   0 _TEXT         0000001a 0000 TEXT
# HDR-NEXT:   1 CONST         00000003 0000 DATA
# HDR-NEXT:   2 _DATA         00000002 0000 DATA
# HDR-NEXT:   3 _BSS          00000004 0000 BSS
# HDR:      SYMBOL TABLE:
# HDR-NEXT: 0000012c       O *COM*	00000000 _buf
# HDR-NEXT: 00000000         *UND* _ext_data
# HDR-NEXT: 00000000         *UND* _ext_func
# HDR-NEXT: 00000000 g     F _TEXT _start
# HDR-NEXT: 00000016 g     F _TEXT _main
# HDR-NEXT: 00000000 g     O _DATA _counter
# HDR-NEXT: 00001234 g     O *ABS* _abs

# DIS:      <_start>:
# DIS-NEXT: e8 13 00 callw 0x16 <_main>
# DIS-NEXT: b8 00 00 movw $0x0, %ax
# DIS-NEXT: 00000004: offset16 _DATA
# DIS-NEXT: b9 04 00 movw $0x4, %cx
# DIS-NEXT: 00000007: offset16 _ext_data
# DIS-NEXT: e8 00 00 callw
# DIS-NEXT: 0000000a: self offset16 _ext_func
# DIS-NEXT: b8 00 00 movw $0x0, %ax
# DIS-NEXT: 0000000d: base16 _DATA
# DIS-NEXT: ba 00 00 movw $0x0, %dx
# DIS-NEXT: 00000010: offset16 CONST
# DIS-NEXT: be 00 00 movw $0x0, %si
# DIS-NEXT: 00000013: offset16 _buf
# DIS-NEXT: c3 retw
# DIS:      <_main>:
# DIS-NEXT: b8 00 00 movw $0x0, %ax
# DIS-NEXT: 00000017: offset16 _BSS

# DATA:      Contents of section CONST:
# DATA-NEXT:  0000 686900 hi.
# DATA-NEXT: Contents of section _DATA:
# DATA-NEXT:  0000 0100 ..

## Defined and communal globals go in the archive map; externals do not.
# ARMAP:      Archive map
# ARMAP-NEXT: _buf in
# ARMAP-NEXT: _start in
# ARMAP-NEXT: _main in
# ARMAP-NEXT: _counter in
# ARMAP-NEXT: _abs in
# ARMAP-EMPTY:

	.file	"omf.c"
	.text
	.globl	_start
_start:
	call	_main
	movw	$_counter, %ax
	movw	$_ext_data+4, %cx
	call	_ext_func
	movw	$_counter@SEG, %ax
	movw	$msg, %dx
	movw	$_buf, %si
	ret

	.globl	_main
_main:
	movw	$local_buf, %ax
	ret

	.section .rodata,"a",@progbits
msg:
	.asciz	"hi"

	.data
	.globl	_counter
	.p2align 1
_counter:
	.short	1

	.local	local_buf
	.comm	local_buf,4,2
	.comm	_buf,300,2

	.globl	_abs
	.set	_abs, 0x1234
