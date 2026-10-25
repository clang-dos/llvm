; The 8086 and 286 have no BT, BTS, BTR or BTC: setting, clearing or flipping
; a variable bit shifts a 1 into place. The 386 has them.
; RUN: llc -mtriple=i8086-unknown-unknown -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=i8086-unknown-unknown -mcpu=i286 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=i8086-unknown-unknown -mcpu=i386 -verify-machineinstrs < %s | FileCheck %s --check-prefix=I386

; CHECK-NOT: bt
; CHECK-LABEL: set:
; CHECK: shlw %cl, %ax
; CHECK-NEXT: orw
define i16 @set(i16 %x, i16 %i) {
  %m = shl i16 1, %i
  %r = or i16 %x, %m
  ret i16 %r
}

; I386-LABEL: clear:
; I386: btrw
; CHECK-LABEL: clear:
; CHECK: rolw %cl, %ax
; CHECK-NEXT: andw
define i16 @clear(i16 %x, i16 %i) {
  %m = shl i16 1, %i
  %n = xor i16 %m, -1
  %r = and i16 %x, %n
  ret i16 %r
}

; CHECK-LABEL: flip:
; CHECK: shlw %cl, %ax
; CHECK-NEXT: xorw
define i16 @flip(i16 %x, i16 %i) {
  %m = shl i16 1, %i
  %r = xor i16 %x, %m
  ret i16 %r
}
; CHECK-NOT: bt
