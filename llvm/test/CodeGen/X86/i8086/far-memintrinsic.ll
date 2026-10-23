; In the near-data models the library's memcpy and memset take near
; pointers, so those of far and huge pointers are expanded into loops.
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=small < %s | FileCheck %s
; In the far-data models the library takes far pointers.
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=compact < %s | FileCheck %s --check-prefix=FAR

; CHECK-LABEL: _set:
; CHECK-NOT: call
; CHECK: movb ${{[0-9]+}}, %es:
; FAR-LABEL: _set:
; FAR: callw _memset
define void @set(ptr addrspace(2) %p, i16 %n) {
  call void @llvm.memset.p2.i16(ptr addrspace(2) %p, i8 0, i16 %n, i1 false)
  ret void
}

; CHECK-LABEL: _copy:
; CHECK-NOT: call
; CHECK: ret
; FAR-LABEL: _copy:
; FAR: callw _memcpy
define void @copy(ptr addrspace(2) %d, ptr %s, i16 %n) {
  call void @llvm.memcpy.p2.p0.i16(ptr addrspace(2) %d, ptr %s, i16 %n, i1 false)
  ret void
}

; A near copy still calls memcpy.
; CHECK-LABEL: _near:
; CHECK: callw _memcpy
define void @near(ptr %d, ptr %s, i16 %n) {
  call void @llvm.memcpy.p0.p0.i16(ptr %d, ptr %s, i16 %n, i1 false)
  ret void
}
