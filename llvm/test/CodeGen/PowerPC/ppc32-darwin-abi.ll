; RUN: llc < %s -mtriple=powerpc-apple-darwin8 -relocation-model=static | FileCheck %s

; 32-bit PowerPC Darwin calling convention: a 24-byte linkage area with LR
; saved at 8(r1), the CR at 4(r1), arguments in r3-r10 and f1-f13 where every
; floating-point argument also uses up GPRs, a char* va_list, and r13-r31,
; f14-f31 and cr2-cr4 callee-saved.

declare void @takes(i32, double, float, i32, double)
declare double @sin(double)
declare void @clobber()

define void @call_args() nounwind {
; CHECK-LABEL: _call_args:
; CHECK:         mflr r0
; CHECK-NEXT:    stwu r1, -64(r1)
; CHECK-DAG:     stw r0, 72(r1)
; CHECK-DAG:     li r3, 1
; CHECK-DAG:     li r7, 4
; CHECK-DAG:     lfs f1,
; CHECK-DAG:     lfs f2,
; CHECK-DAG:     lfs f3,
; CHECK:         bl _takes
; CHECK:         lwz r0, 8(r1)
; CHECK-NEXT:    mtlr r0
; CHECK-NEXT:    blr
  call void @takes(i32 1, double 2.0, float 3.0, i32 4, double 5.0)
  ret void
}

; The ninth word of arguments is on the stack after the 24-byte linkage area
; and the 32 bytes that shadow r3-r10.
define i32 @stack_arg(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e, i32 %f, i32 %g,
                      i32 %h, i32 %i) nounwind {
; CHECK-LABEL: _stack_arg:
; CHECK:         lwz r3, 56(r1)
; CHECK-NEXT:    blr
  ret i32 %i
}

define i32 @varargs(i32 %a, ...) nounwind {
; CHECK-LABEL: _varargs:
; CHECK-DAG:     stw r4, 28(r1)
; CHECK-DAG:     stw r10, 52(r1)
  %ap = alloca ptr
  call void @llvm.va_start(ptr %ap)
  %x = va_arg ptr %ap, i32
  call void @llvm.va_end(ptr %ap)
  ret i32 %x
}

define i64 @add64(i64 %a, i64 %b) nounwind {
; CHECK-LABEL: _add64:
; CHECK:         addc r4, r4, r6
; CHECK-NEXT:    adde r3, r3, r5
; CHECK-NEXT:    blr
  %c = add i64 %a, %b
  ret i64 %c
}

define double @fpr_csr(double %x) nounwind {
; CHECK-LABEL: _fpr_csr:
; CHECK:         stfd f31, 56(r1)
; CHECK:         fmr f31, f1
; CHECK:         bl _sin
; CHECK:         lfd f31, 56(r1)
  %y = call double @sin(double %x)
  %z = fadd double %y, %x
  ret double %z
}

; r13 is an ordinary callee-saved register on Darwin, and r2 is free to use.
define void @gpr_csr() nounwind {
; CHECK-LABEL: _gpr_csr:
; CHECK:         stw r13, -76(r1)
; CHECK:         lwz r13, -76(r1)
  call void asm sideeffect "", "~{r2},~{r13},~{cr2}"()
  ret void
}

; The CR is saved in the linkage area of the caller.
define void @cr_csr() nounwind {
; CHECK-LABEL: _cr_csr:
; CHECK:         mfcr r12
; CHECK-NEXT:    stw r12, 4(r1)
; CHECK:         lwz r12, 4(r1)
; CHECK-NEXT:    mtcrf 32, r12
  call void asm sideeffect "", "~{cr2}"()
  ret void
}

declare void @llvm.va_start(ptr)
declare void @llvm.va_end(ptr)
