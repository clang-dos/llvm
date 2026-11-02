; RUN: llc -mtriple=m68k -mcpu=M68000 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=m68k -mcpu=M68020 -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=M020

; A register copy placed between a compare and its branch must keep the
; condition codes. Code for the 68000 also runs on later CPUs, where MOVE
; from SR is privileged, so it copies through the stack with MOVEM, which
; leaves the condition codes alone. The 68010 and later save CCR with MOVE
; from CCR.

define i32 @len(ptr %s) {
; CHECK-LABEL: len:
; CHECK-NOT: %sr
; CHECK: cmpi.b #0, (0,%a0,%d1)
; CHECK-NEXT: lea (-4,%sp), %sp
; CHECK-NEXT: movem.l %d0, (0,%sp)
; CHECK-NEXT: movem.l (0,%sp), %d1
; CHECK-NEXT: lea (4,%sp), %sp
; CHECK-NEXT: bne
; CHECK-NOT: %sr
; M020-LABEL: len:
; M020: cmpi.b #0, (0,%a0,%d1)
; M020-NEXT: move.w %ccr, -(%sp)
; M020-NEXT: move.l %d0, %d1
; M020-NEXT: move.w (%sp)+, %ccr
; M020-NEXT: bne
entry:
  br label %loop

loop:
  %n = phi i32 [ 0, %entry ], [ %n1, %loop ]
  %p = getelementptr inbounds i8, ptr %s, i32 %n
  %c = load i8, ptr %p
  %n1 = add i32 %n, 1
  %z = icmp eq i8 %c, 0
  br i1 %z, label %done, label %loop

done:
  ret i32 %n
}
