; With data sections, each string and constant goes in a section of its own,
; which the OMF writer makes a COMDAT, so the linker can drop the unused ones.
; OMF linkers do not merge the shared mergeable sections.
; RUN: llc -mtriple=i8086-pc-msdos -data-sections < %s | FileCheck %s
; RUN: llc -mtriple=i8086-pc-msdos < %s | FileCheck %s --check-prefix=MERGE

; CHECK:      .section .rodata.L_.str,"a",@progbits
; CHECK-NEXT: L_.str:
; CHECK:      .section .rodata.L_.str.1,"a",@progbits
; CHECK-NEXT: L_.str.1:
; CHECK:      .section .rodata.L_c,"a",@progbits
; CHECK-NEXT: .p2align 1, 0x0
; CHECK-NEXT: L_c:
; MERGE:      .section .rodata.str1.1,"aMS",@progbits,1
; MERGE:      .section .rodata.cst8,"aM",@progbits,8

@.str = private unnamed_addr constant [4 x i8] c"one\00"
@.str.1 = private unnamed_addr constant [4 x i8] c"two\00"
@c = private unnamed_addr constant [2 x i32] [i32 1, i32 2]

declare void @puts(ptr)

define void @a() {
  call void @puts(ptr @.str)
  call void @puts(ptr @.str.1)
  call void @puts(ptr @c)
  ret void
}
