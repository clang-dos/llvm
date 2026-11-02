; RUN: llc < %s -mtriple=m68k-linux -mcpu=M68000 | FileCheck %s
; RUN: llc < %s -mtriple=m68k-linux -mcpu=M68020 | FileCheck %s --check-prefix=M020

; Before the 68020 a 32-bit product of 16-bit values uses mulu.w or muls.w
; instead of calling __mulsi3.

; CHECK-LABEL: umul16:
; CHECK-NOT: __mulsi3
; CHECK: mulu
; M020-LABEL: umul16:
; M020: muls.l
define i32 @umul16(i16 %a, i16 %b) {
  %x = zext i16 %a to i32
  %y = zext i16 %b to i32
  %p = mul i32 %x, %y
  ret i32 %p
}

; CHECK-LABEL: smul16:
; CHECK-NOT: __mulsi3
; CHECK: muls
define i32 @smul16(i16 %a, i16 %b) {
  %x = sext i16 %a to i32
  %y = sext i16 %b to i32
  %p = mul i32 %x, %y
  ret i32 %p
}

; CHECK-LABEL: smul16_imm:
; CHECK-NOT: __mulsi3
; CHECK: muls
define i32 @smul16_imm(i16 %a) {
  %x = sext i16 %a to i32
  %p = mul i32 %x, 1000
  ret i32 %p
}

; One unsigned 16-bit operand: two mulu.w and an add.
; CHECK-LABEL: umul16x32:
; CHECK-NOT: __mulsi3
; CHECK: mulu
; CHECK: mulu
; CHECK: add.l
define i32 @umul16x32(i16 %a, i32 %b) {
  %x = zext i16 %a to i32
  %p = mul i32 %b, %x
  ret i32 %p
}

; CHECK-LABEL: mul32:
; CHECK: jsr __mulsi3
define i32 @mul32(i32 %a, i32 %b) {
  %p = mul i32 %a, %b
  ret i32 %p
}
