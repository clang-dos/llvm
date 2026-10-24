; A 16-bit Windows DLL runs on its caller's stack, so SS is not DS. On
; win16, locals reached through BX, SI or DI get an SS override, far
; pointers to locals take SS as their segment, and a memcpy or memset of
; the stack is a loop through a far pointer. On DOS, SS is DS.
; RUN: llc -mtriple=i8086-pc-win16 -target-abi=small < %s | FileCheck %s --check-prefixes=CHECK,WIN
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=small < %s | FileCheck %s --check-prefixes=CHECK,DOS

declare void @use_far(ptr addrspace(2))
declare void @use(ptr)

; BP plus an index uses SS already.
; CHECK-LABEL: _index:
; CHECK:       movw -{{[0-9]+}}(%bp,%si), %ax
define i16 @index(i16 %i) {
  %a = alloca [8 x i16], align 2
  call void @use(ptr %a)
  %p = getelementptr inbounds [8 x i16], ptr %a, i16 0, i16 %i
  %v = load i16, ptr %p, align 2
  ret i16 %v
}

; An access through a register to a local, which is not BP-relative.
; CHECK-LABEL: _byreg:
; WIN:         movw %ss:(%si), %ax
; DOS:         movw (%si), %ax
define i16 @byreg(i1 %c) {
  %a = alloca i16, align 2
  %b = alloca i16, align 2
  call void @use(ptr %a)
  call void @use(ptr %b)
  %p = select i1 %c, ptr %a, ptr %b
  %v = load i16, ptr %p, align 2
  ret i16 %v
}

; No override on a frame index: BP already uses SS.
; CHECK-LABEL: _frame:
; CHECK-NOT:   %ss:
; CHECK:       retw
define i16 @frame() {
  %a = alloca i16, align 2
  call void @use(ptr %a)
  %v = load i16, ptr %a, align 2
  ret i16 %v
}

; CHECK-LABEL: _farlocal:
; CHECK:       movw %ss, %[[SEG:[a-z]+]]
; CHECK:       pushw %[[SEG]]
; CHECK:       callw _use_far
define void @farlocal() {
  %a = alloca [4 x i16], align 2
  %f = addrspacecast ptr %a to ptr addrspace(2)
  call void @use_far(ptr addrspace(2) %f)
  ret void
}

; CHECK-LABEL: _copy:
; WIN-NOT:     _memcpy
; WIN:         movw %ss,
; WIN:         movb %es:
; WIN-NOT:     _memcpy
; DOS:         callw _memcpy
; CHECK:       retw
define void @copy() {
  %a = alloca [40 x i16], align 2
  %b = alloca [40 x i16], align 2
  call void @use(ptr %a)
  call void @llvm.memcpy.p0.p0.i16(ptr align 2 %b, ptr align 2 %a, i16 80, i1 false)
  call void @use(ptr %b)
  ret void
}

; A few bytes become loads and stores, which get SS like other accesses.
; CHECK-LABEL: _copy4:
; CHECK-NOT:   _memcpy
; CHECK:       retw
define void @copy4(i1 %c) {
  %a = alloca [2 x i16], align 2
  %b = alloca [2 x i16], align 2
  call void @use(ptr %a)
  call void @llvm.memcpy.p0.p0.i16(ptr align 2 %b, ptr align 2 %a, i16 4, i1 false)
  call void @use(ptr %b)
  ret void
}

; CHECK-LABEL: _clear:
; WIN-NOT:     _memset
; WIN:         movw %ss,
; DOS:         callw _memset
; CHECK:       retw
define void @clear() {
  %a = alloca [40 x i16], align 2
  call void @llvm.memset.p0.i16(ptr align 2 %a, i8 0, i16 80, i1 false)
  call void @use(ptr %a)
  ret void
}

; A memcpy from a global to the stack: only the stack side is far.
; CHECK-LABEL: _init:
; WIN-NOT:     _memcpy
; DOS:         callw _memcpy
; CHECK:       retw
@g = internal constant [40 x i16] zeroinitializer
define void @init() {
  %a = alloca [40 x i16], align 2
  call void @llvm.memcpy.p0.p0.i16(ptr align 2 %a, ptr align 2 @g, i16 80, i1 false)
  call void @use(ptr %a)
  ret void
}

; A memmove between the stack and DGROUP: both sides become far.
; CHECK-LABEL: _move:
; WIN-NOT:     _memmove
; DOS:         callw _memmove
; CHECK:       retw
define void @move(ptr %p) {
  %a = alloca [40 x i16], align 2
  call void @use(ptr %a)
  call void @llvm.memmove.p0.p0.i16(ptr align 2 %p, ptr align 2 %a, i16 80, i1 false)
  ret void
}
