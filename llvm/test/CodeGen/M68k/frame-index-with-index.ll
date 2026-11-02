; RUN: llc < %s -mtriple=m68k -O2 | FileCheck %s

; A store into a stack array at a variable index plus a constant must keep
; the index register. The frame index used to swallow it, so every store
; went to the start of the array.

declare void @use(ptr)

define void @store_at(i32 %i) {
; CHECK-LABEL: store_at:
; CHECK: move.b #7, (5,%a{{[0-9]}},%{{[ad][0-9]}})
entry:
  %buf = alloca [16 x i8], align 1
  %idx = add i32 %i, 5
  %p = getelementptr inbounds i8, ptr %buf, i32 %idx
  store i8 7, ptr %p, align 1
  call void @use(ptr %buf)
  ret void
}
