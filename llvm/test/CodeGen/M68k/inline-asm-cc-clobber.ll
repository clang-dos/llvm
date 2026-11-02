; RUN: llc -mtriple=m68k < %s | FileCheck %s

; An asm statement that clobbers "cc" (a Mac OS trap, which returns with
; the condition codes set from its result) must not come between a
; compare and the branch that tests it.

define void @f(ptr %q, ptr %p) {
; CHECK-LABEL: f:
; CHECK:       cmpi.l #0, (%a0)
; CHECK-NEXT:  move.w %sr, [[SAVE:%d[0-7]]]
; CHECK-NOT:   b{{le|gt}}
; CHECK:       moveq #0, %d0
; CHECK-NOT:   b{{le|gt}}
; CHECK:       move.w [[SAVE]], %ccr
; CHECK-NEXT:  ble
entry:
  %x = load volatile i32, ptr %q
  %cmp = icmp slt i32 %x, 1
  tail call void asm sideeffect "moveq #0, %d0", "~{d0},~{cc}"()
  br i1 %cmp, label %then, label %end
then:
  store i32 1, ptr %p
  br label %end
end:
  ret void
}
