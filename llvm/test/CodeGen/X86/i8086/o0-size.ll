; Unoptimized code has to fit in 64 KiB in the near-code models, so a few
; size passes run at -O0 too: arguments are pushed, not stored, jumps to
; the next block are removed, and a far pointer to a local is made where
; it is used instead of being kept across blocks.
; RUN: llc -O0 -mtriple=i8086-pc-msdos -target-abi=small < %s | FileCheck %s

declare void @f(i16, i16)
declare void @g(ptr addrspace(2))

; CHECK-LABEL: _args:
; CHECK:       movw $2, %ax
; CHECK-NEXT:  pushw %ax
; CHECK-NEXT:  movw $1, %ax
; CHECK-NEXT:  pushw %ax
; CHECK-NEXT:  callw _f
; CHECK-NEXT:  addw $4, %sp
define void @args() {
  call void @f(i16 1, i16 2)
  ret void
}

; CHECK-LABEL: _fall:
; CHECK:       jne .LBB1_2
; CHECK-NOT:   jmp
; CHECK:       .LBB1_2:
define void @fall(i16 %x) {
entry:
  %c = icmp eq i16 %x, 0
  br i1 %c, label %a, label %b
a:
  call void @f(i16 1, i16 1)
  br label %b
b:
  ret void
}

; Each call gets SS and the offset afresh, with nothing spilled.
; CHECK-LABEL: _farlocal:
; CHECK:       je .LBB2_2
; CHECK:       movw %ss, %cx
; CHECK-NEXT:  leaw -2(%bp), %ax
; CHECK:       callw _g
; CHECK:       .LBB2_2:
; CHECK-NEXT:  movw %ss, %cx
; CHECK-NEXT:  leaw -2(%bp), %ax
; CHECK:       callw _g
define void @farlocal(i1 %c) {
entry:
  %v = alloca i16
  %p = addrspacecast ptr %v to ptr addrspace(2)
  br i1 %c, label %a, label %b
a:
  call void @g(ptr addrspace(2) %p)
  br label %b
b:
  call void @g(ptr addrspace(2) %p)
  ret void
}

; A PHI's use of the far pointer is in its incoming block: the copy for the
; call in %m goes after the PHI, not before it.
; CHECK-LABEL: _farlocal_phi:
; CHECK:       retw
define void @farlocal_phi(i1 %c) {
entry:
  %v = alloca i16
  %p = addrspacecast ptr %v to ptr addrspace(2)
  br i1 %c, label %x, label %m
x:
  br label %m
m:
  %q = phi ptr addrspace(2) [ %p, %x ], [ null, %entry ]
  call void @g(ptr addrspace(2) %q)
  call void @g(ptr addrspace(2) %p)
  ret void
}
