; An i8 division by a constant becomes an 8-bit multiply that leaves the
; high half in AH. Selecting it used to crash with "Cannot emit physreg
; copy instruction".
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=small < %s | FileCheck %s

; CHECK-LABEL: _div3:
; CHECK:       movb $-85, %al
; CHECK-NEXT:  mulb 4(%bp)
; CHECK:       %ah
; CHECK:       retw
define i8 @div3(i8 %x) {
  %r = udiv i8 %x, 3
  ret i8 %r
}

; CHECK-LABEL: _sdiv7:
; CHECK:       imulb
; CHECK:       retw
define i8 @sdiv7(i8 %x) {
  %r = sdiv i8 %x, 7
  ret i8 %r
}
