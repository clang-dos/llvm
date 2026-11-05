; RUN: llc -mtriple=i8086-pc-msdos < %s | FileCheck %s
; RUN: llc -mtriple=i8086-pc-msdos -mcpu=i286 < %s | FileCheck %s
; RUN: llc -mtriple=i8086-pc-msdos -mcpu=i486 < %s | FileCheck %s --check-prefix=I486

; Before the 80486 there is no XADD or CMPXCHG: every atomic operation is a
; call to compiler-rt's __atomic_* functions. DOS runs on one processor, so a
; fence only stops the compiler.

@g = global i16 0

; CHECK-LABEL: add:
; CHECK: callw ___atomic_fetch_add_2
; I486-LABEL: add:
; I486: lock xaddw
define i16 @add() {
  %v = atomicrmw add ptr @g, i16 1 seq_cst
  ret i16 %v
}

; CHECK-LABEL: max:
; CHECK: callw ___atomic_compare_exchange_2
define i16 @max(i16 %x) {
  %v = atomicrmw max ptr @g, i16 %x seq_cst
  ret i16 %v
}

; CHECK-LABEL: fence:
; CHECK-NOT: lock
; CHECK: retw
define void @fence() {
  fence seq_cst
  ret void
}
