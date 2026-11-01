; RUN: llc -mtriple=m68k-apple-macos < %s -o - | FileCheck %s

; Classic Mac OS Pascal convention: arguments are pushed left to right, so
; the first argument is highest; bytes take a word (value in the high byte),
; shorts 2 bytes, longs 4. The callee pops the arguments and leaves the
; result in a slot above them, which the caller pops.

declare m68k_pascalcc i16 @callee(i8, i16, i32)

; CHECK-LABEL: caller:
; CHECK-DAG:     move.b #1, (6,%sp)
; CHECK-DAG:     move.w #2, (4,%sp)
; CHECK-DAG:     move.l #3, (%sp)
; CHECK:         jsr callee
; CHECK-NEXT:    move.w (%sp)+, %d0
; CHECK-NEXT:    suba.l #10, %sp
define i16 @caller() {
  %r = call m68k_pascalcc i16 @callee(i8 1, i16 2, i32 3)
  ret i16 %r
}

; A Toolbox trap: the call is the trap word.
declare m68k_pascalcc i8 @Button() "m68k-inline"="A974"
declare m68k_pascalcc void @MoveTo(i16, i16) "m68k-inline"="A893"

; CHECK-LABEL: traps:
; CHECK:         .short 43380
; CHECK-NEXT:    move.b (%sp), %d0
; CHECK-NEXT:    adda.l #2, %sp
; CHECK-NEXT:    suba.l #2, %sp
; CHECK:         move.l #2621460, (%sp)
; CHECK-NEXT:    .short 43155
; CHECK-NEXT:    suba.l #4, %sp
define i8 @traps() {
  %b = call m68k_pascalcc i8 @Button()
  call m68k_pascalcc void @MoveTo(i16 20, i16 40)
  ret i8 %b
}

; A Pascal function: the first argument is highest, above the return
; address; the result goes in the slot above the arguments, and the
; function pops its 8 bytes of arguments.
; CHECK-LABEL: sub:
; CHECK:         move.l (8,%sp), %d0
; CHECK-NEXT:    sub.l (4,%sp), %d0
; CHECK-NEXT:    move.l %d0, (12,%sp)
; CHECK-NEXT:    move.l (%sp), %a1
; CHECK-NEXT:    adda.l #8, %sp
; CHECK-NEXT:    move.l %a1, (%sp)
; CHECK-NEXT:    rts
define m68k_pascalcc i32 @sub(i32 %a, i32 %b) {
  %d = sub i32 %a, %b
  ret i32 %d
}

; A5 holds the application globals on classic Mac OS: never allocate it.
; CHECK-LABEL: pressure:
; CHECK-NOT:     %a5
; CHECK:         rts
define void @pressure(ptr %p) {
  %v0 = load volatile i32, ptr %p
  %v1 = load volatile i32, ptr %p
  %v2 = load volatile i32, ptr %p
  %v3 = load volatile i32, ptr %p
  %v4 = load volatile i32, ptr %p
  %v5 = load volatile i32, ptr %p
  %v6 = load volatile i32, ptr %p
  %v7 = load volatile i32, ptr %p
  %v8 = load volatile i32, ptr %p
  %v9 = load volatile i32, ptr %p
  %va = load volatile i32, ptr %p
  %vb = load volatile i32, ptr %p
  %vc = load volatile i32, ptr %p
  %vd = load volatile i32, ptr %p
  %ve = load volatile i32, ptr %p
  store volatile i32 %v0, ptr %p
  store volatile i32 %v1, ptr %p
  store volatile i32 %v2, ptr %p
  store volatile i32 %v3, ptr %p
  store volatile i32 %v4, ptr %p
  store volatile i32 %v5, ptr %p
  store volatile i32 %v6, ptr %p
  store volatile i32 %v7, ptr %p
  store volatile i32 %v8, ptr %p
  store volatile i32 %v9, ptr %p
  store volatile i32 %va, ptr %p
  store volatile i32 %vb, ptr %p
  store volatile i32 %vc, ptr %p
  store volatile i32 %vd, ptr %p
  store volatile i32 %ve, ptr %p
  ret void
}
