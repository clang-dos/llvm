; RUN: llc -mtriple=m68k < %s | FileCheck %s

; The SLP vectorizer can make vector divisions (here of a rectangle's
; width and height by 2), which the M68k has no instructions for. The
; DAG combiner's comparisons for them need a vector result type.

; CHECK-LABEL: halve:
; CHECK: asr.w
; CHECK: asr.w
; CHECK: rts
define <2 x i16> @halve(<2 x i16> %v) {
  %r = sdiv <2 x i16> %v, <i16 2, i16 2>
  ret <2 x i16> %r
}
