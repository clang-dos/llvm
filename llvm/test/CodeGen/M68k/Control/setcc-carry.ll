; RUN: llc -mtriple=m68k < %s | FileCheck %s

; A 0/-1 value from a compare's carry must not come from subx, which reads
; the X flag that cmp does not set. (err ? -1 : value, after a trap.)

define i32 @f(i16 %e, i32 %v) {
; CHECK-LABEL: f:
; CHECK:       cmpi.w #0, (6,%sp)
; CHECK-NOT:   subx
; CHECK:       moveq #-1, %d0
; CHECK-NOT:   subx
; CHECK:       .Lfunc_end0:
  %c = icmp ne i16 %e, 0
  %r = select i1 %c, i32 -1, i32 %v
  ret i32 %r
}
