; i8086 huge pointers (address space 3): as integers they are linear
; addresses, so conversions and comparisons go through segment * 16 +
; offset, and arithmetic carries into the segment and normalizes the
; result (offset 0 to 15). A small constant offset that is only loaded
; from or stored to is added to the offset.
; RUN: opt -passes=x86-huge-pointers -S < %s | FileCheck %s --check-prefix=IR
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=huge < %s | FileCheck %s
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=huge16 < %s | FileCheck %s --check-prefix=HUGE16

target triple = "i8086-pc-msdos"

define ptr addrspace(3) @index(ptr addrspace(3) %p, i32 %i) nounwind {
; IR-LABEL: @index(
; IR:         [[RAW:%.*]] = ptrtoint ptr addrspace(3) %p to i32
; IR:         [[OFF:%.*]] = trunc i32 [[RAW]] to i16
; IR:         [[SHR:%.*]] = lshr i32 [[RAW]], 16
; IR:         [[SEG:%.*]] = trunc i32 [[SHR]] to i16
; IR:         [[OFF32:%.*]] = zext i16 [[OFF]] to i32
; IR:         [[T:%.*]] = add i32 [[OFF32]], %i
; IR:         [[CARRY:%.*]] = ashr i32 [[T]], 4
; IR:         [[CARRY16:%.*]] = trunc i32 [[CARRY]] to i16
; IR:         [[NSEG:%.*]] = add i16 [[SEG]], [[CARRY16]]
; IR:         [[LOW:%.*]] = and i32 [[T]], 15
; IR:         [[NOFF:%.*]] = trunc i32 [[LOW]] to i16
; IR:         inttoptr i32 %{{.*}} to ptr addrspace(3)
; CHECK-LABEL: index:
; CHECK:       andw $15,
; CHECK:       lretw
  %q = getelementptr i8, ptr addrspace(3) %p, i32 %i
  ret ptr addrspace(3) %q
}

define i16 @field(ptr addrspace(3) %p) nounwind {
; IR-LABEL: @field(
; IR:         add i16 %{{.*}}, 2
; IR-NOT:     ashr
; IR:         load i16
; CHECK-LABEL: field:
; CHECK:       movw %es:2(%bx), %ax
  %q = getelementptr i8, ptr addrspace(3) %p, i32 2
  %v = load i16, ptr addrspace(3) %q, align 2
  ret i16 %v
}

define i32 @diff(ptr addrspace(3) %a, ptr addrspace(3) %b) nounwind {
; IR-LABEL: @diff(
; IR:         shl i32 %{{.*}}, 4
; IR:         shl i32 %{{.*}}, 4
; IR:         sub i32
  %x = ptrtoaddr ptr addrspace(3) %a to i32
  %y = ptrtoaddr ptr addrspace(3) %b to i32
  %d = sub i32 %x, %y
  ret i32 %d
}

define i1 @less(ptr addrspace(3) %a, ptr addrspace(3) %b) nounwind {
; IR-LABEL: @less(
; IR:         [[LA:%.*]] = add i32
; IR:         [[LB:%.*]] = add i32
; IR:         icmp ult i32 [[LA]], [[LB]]
  %c = icmp ult ptr addrspace(3) %a, %b
  ret i1 %c
}

define ptr addrspace(3) @from_linear(i32 %l) nounwind {
; IR-LABEL: @from_linear(
; IR:         lshr i32 %l, 4
; IR:         and i32 %l, 15
  %p = inttoptr i32 %l to ptr addrspace(3)
  ret ptr addrspace(3) %p
}

; Memory library calls take huge pointers and a 32-bit size.
declare void @llvm.memset.p3.i32(ptr addrspace(3), i8, i32, i1)
define void @fill(ptr addrspace(3) %p, i32 %n) nounwind {
; CHECK-LABEL: fill:
; CHECK:       pushw 12(%bp)
; CHECK-NEXT:  pushw 10(%bp)
; CHECK:       lcallw $_memset@SEG, $_memset
; CHECK-NEXT:  addw $10, %sp
; With a 16-bit size_t (huge16) the size is one word.
; HUGE16-LABEL: fill:
; HUGE16:       pushw 10(%bp)
; HUGE16-NEXT:  movw $1, %ax
; HUGE16-NEXT:  pushw %ax
; HUGE16-NEXT:  pushw 8(%bp)
; HUGE16-NEXT:  pushw 6(%bp)
; HUGE16-NEXT:  lcallw $_memset@SEG, $_memset
; HUGE16-NEXT:  addw $8, %sp
  call void @llvm.memset.p3.i32(ptr addrspace(3) %p, i8 1, i32 %n, i1 false)
  ret void
}
