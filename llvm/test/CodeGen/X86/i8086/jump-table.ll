; Jump tables go in the function's own code section and are read through CS,
; so that the linker keeps a table only with its function.
; RUN: llc -mtriple=i8086-pc-msdos -O0 < %s | FileCheck %s
; RUN: llc -mtriple=i8086-pc-msdos -O0 -function-sections < %s \
; RUN:   | FileCheck %s --check-prefix=FS

; CHECK-LABEL: _sw:
; CHECK:       movw %cs:LJTI0_0(%bx), %ax
; CHECK-NOT:   .section
; CHECK:       LJTI0_0:
; CHECK-NEXT:  .short .LBB0_
; FS:          .section .text._sw,"ax",@progbits
; FS-NOT:      .section
; FS:          LJTI0_0:

define i16 @sw(i16 %x) {
entry:
  switch i16 %x, label %def [
    i16 0, label %a
    i16 1, label %b
    i16 2, label %c
    i16 3, label %d
    i16 4, label %e
  ]
a:
  ret i16 11
b:
  ret i16 22
c:
  ret i16 37
d:
  ret i16 41
e:
  ret i16 59
def:
  ret i16 -1
}
