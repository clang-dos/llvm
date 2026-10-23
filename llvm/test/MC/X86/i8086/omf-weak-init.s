# Undefined weak symbols become WKEXT weak externals that default to
# __omf_weak_null. .init_array and .fini_array go to the word-aligned XI and
# YI segments, which are written together with XIB, XIE, YIB and YIE.
# RUN: llvm-mc -triple=i8086-pc-msdos -filetype=obj %s -o %t.obj
# RUN: %python %S/Inputs/omfdump.py %t.obj | FileCheck %s

# CHECK:      SEGDEF XIB align=word combine=public big=0 use32=0 len=0000 class=DATA
# CHECK-NEXT: SEGDEF XI align=word combine=public big=0 use32=0 len=0002 class=DATA
# CHECK-NEXT: SEGDEF XIE align=word combine=public big=0 use32=0 len=0000 class=DATA
# CHECK-NEXT: SEGDEF YIB align=word combine=public big=0 use32=0 len=0000 class=DATA
# CHECK-NEXT: SEGDEF YI align=word combine=public big=0 use32=0 len=0004 class=DATA
# CHECK-NEXT: SEGDEF YIE align=word combine=public big=0 use32=0 len=0000 class=DATA
# CHECK:      EXTDEF '_weak_func' type=0
# CHECK-NEXT: EXTDEF '_strong' type=0
# CHECK-NEXT: EXTDEF '_weak_data' type=0
# CHECK-NEXT: EXTDEF '__omf_weak_null' type=0
# CHECK-NEXT: COMENT class=A8 b'\x01\x04\x03\x04'
# CHECK:      PUBDEF '_weak_def'

	.text
	.weak	_weak_func
	.weak	_weak_data
	.weak	_weak_def
	.weak	_unused
f:
	movw	$_weak_func, %ax
	callw	_strong
	movw	_weak_data, %ax
	retw
_weak_def:
	retw

	.section .init_array,"aw",@init_array
	.short	f
	.section .fini_array.100,"aw",@fini_array
	.short	f
	.short	f
