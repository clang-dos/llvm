# RUN: not llvm-mc -triple=i8086-pc-msdos -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

	.text
a:
	nop
# CHECK: :[[#@LINE+1]]:{{[0-9]+}}: error: unsupported relocation size in OMF object; only 16-bit offsets are supported
	.long	ext
# CHECK: :[[#@LINE+1]]:{{[0-9]+}}: error: unsupported relocation expression in OMF object
	.short	b - a
	.data
b:
	.byte	0
