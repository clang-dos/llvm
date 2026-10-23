; Globals in the far (2) and huge (3) address spaces each go in a section of
; their own, which the OMF writer makes a far segment outside DGROUP.
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=compact < %s | FileCheck %s
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=huge < %s | FileCheck %s

; CHECK-LABEL: _get:
; CHECK: movw $_zero@SEG, %ax
; CHECK: movw %ax, %es
; CHECK: movb %es:_zero+10, %al
define i8 @get() {
  %v = load i8, ptr addrspace(2) getelementptr (i8, ptr addrspace(2) @zero, i16 10)
  ret i8 %v
}

; CHECK: .section .fardata._init,"aw",@progbits
; CHECK: _init:
@init = addrspace(2) global [3 x i16] [i16 1, i16 2, i16 3]
; CHECK: .section .farbss._zero,"aw",@nobits
; CHECK: _zero:
@zero = addrspace(2) global [40000 x i8] zeroinitializer
; CHECK: .section .farrodata._ro,"a",@progbits
; CHECK: _ro:
@ro = addrspace(3) constant [3 x i8] c"ro\00"
; Other globals stay in DGROUP.
; CHECK: .data
; CHECK: _near:
@near = global i16 5
; A pointer to far data is a far pointer fixup.
; CHECK: _pz:
; CHECK-NEXT: .long _zero+4
@pz = global ptr addrspace(2) getelementptr (i8, ptr addrspace(2) @zero, i16 4)
