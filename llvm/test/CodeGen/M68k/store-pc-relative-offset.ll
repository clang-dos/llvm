; RUN: llc < %s -mtriple=m68k-apple-macos | FileCheck %s

; A store cannot use a PC-relative address, so a store to a global plus an
; offset computes the address into a register first. This used to fail
; instruction selection.

@tup = internal global [100 x i8] zeroinitializer, align 1

define void @f() {
; CHECK-LABEL: f:
; CHECK: lea (tup,%pc), %a0
; CHECK: add.l #1, %d0
; CHECK: move.l %d0, %a0
; CHECK: move.b #52, (%a0)
  store i8 52, ptr getelementptr inbounds nuw (i8, ptr @tup, i32 1), align 1
  ret void
}
