; RUN: llc -mtriple=m68k < %s | FileCheck %s

; A 0/-1 value from a compare's carry must come from scs: subx would read
; the X flag, which cmp does not set. (err ? -1 : value, after a trap.)

define i32 @f(i16 %e, i32 %v) {
; CHECK-LABEL: f:
; CHECK:       cmpi.w #1, (6,%sp)
; CHECK-NEXT:  scs %d1
; CHECK-NEXT:  ext.w %d1
; CHECK-NEXT:  ext.l %d1
; CHECK-NOT:   subx
; CHECK:       rts
  %c = icmp ne i16 %e, 0
  %r = select i1 %c, i32 -1, i32 %v
  ret i32 %r
}
