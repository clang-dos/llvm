# A C++ type_info object or its name in a section of its own is a "pick any"
# COMDAT even outside a group: code built with -fno-rtti emits its own copy,
# in a group, of one that the C++ runtime defines, and the linker must merge
# them. Other sections of their own outside a group stay "no match".
# RUN: llvm-mc -triple=i8086-pc-msdos -filetype=obj %s -o %t.obj
# RUN: %python %S/Inputs/omfdump.py %t.obj | FileCheck %s

# CHECK: COMDAT '__ZTISt9exception' flags=00 attr=10
# CHECK: COMDAT '__ZTSSt9exception' flags=00 attr=10
# CHECK: COMDAT '_table' flags=00 attr=00

	.section .rodata.__ZTISt9exception,"a",@progbits
	.globl __ZTISt9exception
__ZTISt9exception:
	.short __ZTSSt9exception

	.section .rodata.__ZTSSt9exception,"a",@progbits
	.globl __ZTSSt9exception
__ZTSSt9exception:
	.asciz "St9exception"

	.section .rodata._table,"a",@progbits
	.globl _table
_table:
	.short 1
