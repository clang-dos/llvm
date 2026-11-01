; Classic Mac OS on PowerPC uses the 32-bit AIX ABI: functions have TVector
; descriptors, globals are reached through the TOC in r2, objects are XCOFF.

; RUN: llc -mtriple=powerpc-apple-macos9.2 < %s | FileCheck %s
; RUN: llc -mtriple=powerpc-apple-macosclassic < %s | FileCheck %s
; RUN: llc -mtriple=powerpc-apple-macos9.2 -filetype=obj -o %t.o < %s
; RUN: llvm-readobj --file-headers %t.o | FileCheck %s --check-prefix=OBJ

; The default CPU is the 601, which every PowerPC Mac can run.
; CHECK: .machine "601"

@counter = global i32 0, align 4

declare void @SetRect(ptr, i16 signext, i16 signext, i16 signext, i16 signext)

; CHECK: .globl bump[DS]
; CHECK: .csect bump[DS],2
; CHECK-NEXT: .vbyte 4, .bump
; CHECK-NEXT: .vbyte 4, TOC[TC0]
; CHECK-NEXT: .vbyte 4, 0
; CHECK: .bump:
; CHECK: lwz [[REG:[0-9]+]], L..C0(2)
; CHECK: bl .SetRect[PR]
; CHECK-NEXT: nop
define i32 @bump(ptr %r) {
entry:
  %v = load i32, ptr @counter, align 4
  %n = add i32 %v, 1
  store i32 %n, ptr @counter, align 4
  call void @SetRect(ptr %r, i16 signext 1, i16 signext 2, i16 signext 3, i16 signext 4)
  ret i32 %n
}

; CHECK: .toc
; CHECK: L..C0:
; CHECK-NEXT: .tc counter[TC],counter[RW]

; OBJ: Format: aixcoff-rs6000
; OBJ: Magic: 0x1DF
