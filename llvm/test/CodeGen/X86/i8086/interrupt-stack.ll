; An i8086 interrupt function runs on the stack of the program it
; interrupted, so SS is not DS: stores of outgoing call arguments through a
; copy of SP get an SS override, and accesses through BP need none.
; RUN: llc -mtriple=i8086-pc-msdos -O0 -no-x86-call-frame-opt < %s | FileCheck %s

declare void @use(i16, i16)

; CHECK-LABEL: _args:
; CHECK:       movw 22(%bp), %ax
; CHECK:       movw %ax, -8(%bp,%si)
; CHECK:       movw %sp, %bx
; CHECK:       movw %ax, %ss:2(%si)
; CHECK-NEXT:  movw $1, %ss:(%bx)
; CHECK-NEXT:  callw _use
define x86_intrcc void @args(ptr %r) addrspace(4) {
entry:
  %a = alloca [4 x i16], align 2
  %p = getelementptr inbounds i8, ptr %r, i16 22
  %v = load volatile i16, ptr %p, align 2
  %i = and i16 %v, 3
  %e = getelementptr inbounds [4 x i16], ptr %a, i16 0, i16 %i
  store volatile i16 %v, ptr %e, align 2
  call addrspace(4) void @use(i16 1, i16 %v)
  ret void
}

; A normal function keeps DS for the same stores.
; CHECK-LABEL: _plain:
; CHECK:       movw %sp, %bx
; CHECK:       movw %ax, 2(%si)
; CHECK-NEXT:  movw $1, (%bx)
define void @plain(i16 %v) addrspace(4) {
entry:
  call addrspace(4) void @use(i16 1, i16 %v)
  ret void
}
