; In the i8086 memory models with far data (the target-abi module flag), a
; library call that the optimizer adds takes far pointers (address space 2).
; RUN: opt -passes=instcombine -S < %s | FileCheck %s

target datalayout = "e-m:x-p:16:16-p1:16:16-p2:32:16:16:16-pu3:32:16:16:32-p4:16:16-p5:32:16:16:16-p256:16:16-p257:16:16-p258:16:16-i32:16-i64:16-i128:16-f32:16-f64:16-f80:16-n8:16-a:0:16-S16-P4"
target triple = "i8086-pc-msdos"

@.str = private constant [7 x i8] c"hello\0A\00"

declare i16 @printf(ptr addrspace(2), ...)

; CHECK-LABEL: define void @f(
; CHECK: %puts = call addrspace(4) i16 @puts(ptr addrspace(2) addrspacecast (ptr @str to ptr addrspace(2)))
; CHECK: declare {{.*}}i16 @puts(ptr addrspace(2) noundef{{.*}})
define void @f() {
  %p = addrspacecast ptr @.str to ptr addrspace(2)
  call i16 (ptr addrspace(2), ...) @printf(ptr addrspace(2) %p)
  ret void
}

!llvm.module.flags = !{!0}
!0 = !{i32 1, !"target-abi", !"compact"}
