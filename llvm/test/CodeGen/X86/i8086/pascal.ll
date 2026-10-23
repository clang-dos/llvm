; The pascal convention (x86_16_pascalcc): arguments are pushed left to
; right, so the last one is nearest the return address; the callee removes
; them with RET n; and the symbol is the upper-case name with no underscore.
; An sret pointer stays first. Variadic calls fall back to C rules.
; RUN: llc -mtriple=i8086-pc-msdos < %s | FileCheck %s
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=large < %s | FileCheck %s --check-prefix=FAR

; d is at 4(%bp), c at 6, b at 8 and 10, a at 12.
; CHECK-LABEL: MIX:
; CHECK:       movw 12(%bp), %ax
; CHECK:       retw $10
; FAR-LABEL:   MIX:
; FAR:         movw 14(%bp), %ax
; FAR:         lretw $10
define x86_16_pascalcc i16 @mix(i16 %a, i32 %b, i8 %c, i16 %d) {
  %r = add i16 %a, %d
  ret i16 %r
}

; CHECK-LABEL: SECOND:
; CHECK:       movw 4(%bp), %ax
; CHECK:       retw $4
define x86_16_pascalcc i16 @second(i16 %a, i16 %b) {
  ret i16 %b
}

; The caller pushes the first argument first and does not pop.
; CHECK-LABEL: _caller:
; CHECK:       movw $1, %ax
; CHECK-NEXT:  pushw %ax
; CHECK-NEXT:  movw $2, %ax
; CHECK-NEXT:  pushw %ax
; CHECK-NEXT:  callw SECOND
; CHECK-NOT:   addw ${{[0-9]+}}, %sp
; CHECK:       retw
define i16 @caller() {
  %r = call x86_16_pascalcc i16 @second(i16 1, i16 2)
  ret i16 %r
}

; A pascal call is never a tail call: the callee's RET n would not match the
; caller's frame.
; CHECK-LABEL: SIB:
; CHECK:       callw SECOND
; CHECK:       retw $4
define x86_16_pascalcc i16 @sib(i16 %a, i16 %b) {
  %r = tail call x86_16_pascalcc i16 @second(i16 %b, i16 %a)
  ret i16 %r
}

; A name written with \01 is kept as it is.
; CHECK-LABEL: Raw_Name:
define x86_16_pascalcc void @"\01Raw_Name"() {
  ret void
}

; The sret pointer stays at 4(%bp); x and y follow in pascal order.
; CHECK-LABEL: MK:
; CHECK-DAG:   movw 4(%bp), %[[R:[a-z]+]]
; CHECK-DAG:   movw 8(%bp), %[[X:[a-z]+]]
; CHECK-DAG:   movw 6(%bp), %[[Y:[a-z]+]]
; CHECK:       retw $6
%pair = type { i16, i16 }
define x86_16_pascalcc void @mk(ptr sret(%pair) %r, i16 %x, i16 %y) {
  store i16 %x, ptr %r
  %p = getelementptr i16, ptr %r, i16 1
  store i16 %y, ptr %p
  ret void
}
