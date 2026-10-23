# RUN: not llvm-mc -triple=i8086-pc-msdos -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

	.text
a:
	nop
# CHECK: :[[#@LINE+1]]:{{[0-9]+}}: error: unsupported relocation size in OMF object; only 16-bit offsets and far pointers are supported
	.quad	ext
# CHECK: :[[#@LINE+1]]:{{[0-9]+}}: error: a far pointer cannot be PC-relative or a segment reference
	.long	ext@SEG
# CHECK: :[[#@LINE+1]]:{{[0-9]+}}: error: unsupported relocation expression in OMF object
	.short	b - a
# CHECK: :[[#@LINE+1]]:{{[0-9]+}}: error: unsupported relocation expression in OMF object
	movw	$b@GOTOFF, %ax
	.data
b:
	.byte	0
