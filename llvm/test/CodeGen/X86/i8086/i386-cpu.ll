; RUN: llc -mtriple=i8086-pc-msdos -mcpu=i386 < %s | FileCheck %s

; 16-bit code for an 80386 can use 32-bit registers but still addresses
; memory through BX, BP, SI and DI.

; An index computed in a 32-bit register is copied to SI or DI, not used
; through its 16-bit half.
; CHECK-LABEL: store_index:
; CHECK: movw $5, (%bx,%si)
define void @store_index(ptr %p, i16 %i) {
  %a = getelementptr inbounds [2 x i8], ptr %p, i16 %i
  store i16 5, ptr %a, align 2
  ret void
}

; A 32-bit LEA would take 16-bit address registers and drop the high half,
; so 32-bit sums and products are plain arithmetic.
; CHECK-LABEL: sum3:
; CHECK-NOT: leal {{.*}}(%{{[a-d]x|si|di|bx|bp}}
; CHECK: retw
define i32 @sum3(i32 %a, i32 %b) {
  %s = add i32 %a, %b
  %t = add i32 %s, 6
  ret i32 %t
}

; CHECK-LABEL: mul3:
; CHECK-NOT: leal {{.*}}(%{{[a-d]x|si|di|bx|bp}}
; CHECK: retw
define i32 @mul3(i32 %a) {
  %m = mul i32 %a, 3
  ret i32 %m
}
