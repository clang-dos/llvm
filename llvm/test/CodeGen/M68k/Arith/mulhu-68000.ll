; RUN: llc -mtriple=m68k -mcpu=M68000 < %s | FileCheck %s

; The 68000 has no 32x32->64 multiply or combined 32-bit divide and
; remainder. The high half of a product goes through a 64-bit multiply call,
; and a remainder next to a divide is computed from the quotient, instead of
; failing to select.

define i32 @mulhu(i32 %a, i32 %b) {
; CHECK-LABEL: mulhu:
; CHECK: jsr __muldi3
  %x = zext i32 %a to i64
  %y = zext i32 %b to i64
  %m = mul i64 %x, %y
  %h = lshr i64 %m, 32
  %r = trunc i64 %h to i32
  ret i32 %r
}

define i32 @mulhs(i32 %a, i32 %b) {
; CHECK-LABEL: mulhs:
; CHECK: jsr __muldi3
  %x = sext i32 %a to i64
  %y = sext i32 %b to i64
  %m = mul i64 %x, %y
  %h = ashr i64 %m, 32
  %r = trunc i64 %h to i32
  ret i32 %r
}

define i32 @divrem(i32 %a, i32 %b) {
; CHECK-LABEL: divrem:
; CHECK: jsr __udivsi3
; CHECK: jsr __mulsi3
  %q = udiv i32 %a, %b
  %r = urem i32 %a, %b
  %s = add i32 %q, %r
  ret i32 %s
}
