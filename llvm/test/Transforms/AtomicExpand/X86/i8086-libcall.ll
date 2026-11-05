; RUN: split-file %s %t
; RUN: opt -S -mtriple=i8086-pc-msdos -passes='require<libcall-lowering-info>,atomic-expand' %t/small.ll | FileCheck %s --check-prefix=SMALL
; RUN: opt -S -mtriple=i8086-pc-msdos -passes='require<libcall-lowering-info>,atomic-expand' %t/large.ll | FileCheck %s --check-prefix=LARGE
; RUN: opt -S -mtriple=i8086-pc-msdos -mcpu=i486 -passes='require<libcall-lowering-info>,atomic-expand' %t/small.ll | FileCheck %s --check-prefix=I486

; 16-bit code before the 80486 makes every atomic operation an __atomic_*
; call. The pointers go in the address space of the memory model's C
; pointers, and size_t and the orders are 16 bits.

;--- small.ll
@g = global i16 0
@w = global i32 0, align 2

; SMALL-LABEL: @add(
; SMALL: call {{.*}}i16 @__atomic_fetch_add_2(ptr @g, i16 1, i16 5)
; I486-LABEL: @add(
; I486: atomicrmw add ptr @g, i16 1 seq_cst
define i16 @add() {
  %v = atomicrmw add ptr @g, i16 1 seq_cst
  ret i16 %v
}

; SMALL-LABEL: @load(
; SMALL: call {{.*}}i16 @__atomic_load_2(ptr @g, i16 2)
define i16 @load() {
  %v = load atomic i16, ptr @g acquire, align 2
  ret i16 %v
}

; An underaligned object takes the generic call, with a 16-bit size_t.
; SMALL-LABEL: @load_underaligned(
; SMALL: call {{.*}}void @__atomic_load(i16 4, ptr @w, ptr %{{.*}}, i16 5)
define i32 @load_underaligned() {
  %v = load atomic i32, ptr @w seq_cst, align 2
  ret i32 %v
}

;--- large.ll
@g = global i16 0

; LARGE-LABEL: @cas(
; LARGE: [[E:%.*]] = alloca i16
; LARGE: call zeroext {{.*}}i1 @__atomic_compare_exchange_2(ptr addrspace(2) addrspacecast (ptr @g to ptr addrspace(2)), ptr addrspace(2) %{{.*}}, i16 %b, i16 5, i16 5)
define i1 @cas(i16 %a, i16 %b) {
  %r = cmpxchg ptr @g, i16 %a, i16 %b seq_cst seq_cst
  %ok = extractvalue { i16, i1 } %r, 1
  ret i1 %ok
}

; LARGE-LABEL: @far(
; LARGE: call {{.*}}void @__atomic_store_2(ptr addrspace(2) %p, i16 7, i16 5)
define void @far(ptr addrspace(2) %p) {
  store atomic i16 7, ptr addrspace(2) %p seq_cst, align 2
  ret void
}

!llvm.module.flags = !{!0}
!0 = !{i32 1, !"target-abi", !"large"}
