; A DOS program has one thread: thread-local variables are ordinary ones,
; reached without FS (an 80386 register) and kept out of .tbss and .tdata.
; RUN: llc -mtriple=i8086-pc-msdos < %s | FileCheck %s
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=large < %s | FileCheck %s --check-prefix=LARGE
; RUN: llc -mtriple=i8086-pc-msdos -O0 < %s | FileCheck %s --check-prefix=O0

@counter = thread_local global i16 0
@table = internal thread_local global [4 x i16] [i16 1, i16 2, i16 3, i16 4]

; CHECK-LABEL: _bump:
; CHECK-NOT: %fs
; CHECK-NOT: tls
; CHECK: movw _counter, %ax
; CHECK-NEXT: incw %ax
; CHECK-NEXT: movw %ax, _counter
; CHECK: retw
; LARGE-LABEL: _bump:
; LARGE-NOT: %fs
; LARGE: lretw
; O0-LABEL: _bump:
; O0-NOT: %fs
; O0: retw
define i16 @bump() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @counter)
  %v = load i16, ptr %p
  %n = add i16 %v, 1
  store i16 %n, ptr %p
  ret i16 %n
}

; CHECK-LABEL: _second:
; CHECK-NOT: %fs
; CHECK: movw _table+2, %ax
define i16 @second() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @table)
  %e = getelementptr i16, ptr %p, i16 1
  %v = load i16, ptr %e
  ret i16 %v
}

; CHECK-NOT: .tbss
; CHECK-NOT: .tdata
; CHECK: .bss
; CHECK: _counter:
; CHECK: .data
; CHECK: _table:

declare ptr @llvm.threadlocal.address.p0(ptr)
