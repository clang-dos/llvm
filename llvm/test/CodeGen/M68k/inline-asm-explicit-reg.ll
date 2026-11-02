; RUN: llc -mtriple=m68k < %s | FileCheck %s

; An explicit register for a 16- or 8-bit value (`register short x
; asm("d0")` in C) uses the register's low word or byte.

; CHECK-LABEL: word_d0:
; CHECK: ;APP
; CHECK: ;NO_APP
; CHECK-NEXT: ext.l %d0
define i32 @word_d0() {
  %r = call i16 asm sideeffect "nop", "={d0}"()
  %e = sext i16 %r to i32
  ret i32 %e
}

; CHECK-LABEL: word_a0:
; CHECK: ;NO_APP
; CHECK-NEXT: move.w %a0, %d0
define i16 @word_a0() {
  %r = call i16 asm sideeffect "nop", "={a0}"()
  ret i16 %r
}

; CHECK-LABEL: byte_d1:
; CHECK: ;NO_APP
; CHECK-NEXT: move.b %d1, %d0
define i8 @byte_d1() {
  %r = call i8 asm sideeffect "nop", "={d1}"()
  ret i8 %r
}

; A 16-bit result and a 32-bit operand in the same register.
; CHECK-LABEL: word_out_long_in:
; CHECK: move.l (4,%sp), %d0
; CHECK: ;APP
define i16 @word_out_long_in(i32 %x) {
  %r = call i16 asm sideeffect "nop", "={d0},{d0}"(i32 %x)
  ret i16 %r
}
