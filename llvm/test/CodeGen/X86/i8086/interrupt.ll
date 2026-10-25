; i8086 interrupt functions (x86_intrcc, clang's __interrupt): every register
; saved in Open Watcom's order, DGROUP loaded into DS (CS in tiny), IRET.
; The parameter is the saved registers: a pointer to them (union INTPACK),
; or Microsoft C's es, ds, di, ... A call to an interrupt function pushes
; the flags and makes a far call.
; RUN: llc -mtriple=i8086-pc-msdos < %s | FileCheck %s
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=tiny < %s | FileCheck %s --check-prefix=TINY

@g = global i16 0
@old = global ptr addrspace(5) null

; CHECK-LABEL: _regs:
; CHECK:       # %bb.0:
; CHECK-NEXT:  pushw %ax
; CHECK-NEXT:  pushw %cx
; CHECK-NEXT:  pushw %dx
; CHECK-NEXT:  pushw %bx
; CHECK-NEXT:  pushw %sp
; CHECK-NEXT:  pushw %bp
; CHECK-NEXT:  pushw %si
; CHECK-NEXT:  pushw %di
; CHECK-NEXT:  pushw %ds
; CHECK-NEXT:  pushw %es
; CHECK-NEXT:  pushw %ax
; CHECK-NEXT:  pushw %ax
; CHECK-NEXT:  movw %sp, %bp
; CHECK-NEXT:  cld
; CHECK-NEXT:  movw $DGROUP@SEG, %ax
; CHECK-NEXT:  movw %ax, %ds
; CHECK-NEXT:  movw 16(%bp), %ax
; CHECK-NEXT:  addw %ax, 22(%bp)
; CHECK-NEXT:  popw %ax
; CHECK-NEXT:  popw %ax
; CHECK-NEXT:  popw %es
; CHECK-NEXT:  popw %ds
; CHECK-NEXT:  popw %di
; CHECK-NEXT:  popw %si
; CHECK-NEXT:  popw %bp
; CHECK-NEXT:  popw %bx
; CHECK-NEXT:  popw %bx
; CHECK-NEXT:  popw %dx
; CHECK-NEXT:  popw %cx
; CHECK-NEXT:  popw %ax
; CHECK-NEXT:  iretw
; TINY-LABEL: _regs:
; TINY:        movw %cs, %ax
; TINY-NEXT:   movw %ax, %ds
define x86_intrcc void @regs(ptr %r) addrspace(5) {
  %bxp = getelementptr i8, ptr %r, i16 16
  %axp = getelementptr i8, ptr %r, i16 22
  %bx = load i16, ptr %bxp
  %ax = load i16, ptr %axp
  %s = add i16 %ax, %bx
  store i16 %s, ptr %axp
  ret void
}

; Microsoft C register parameters: es at [bp+4], ds at [bp+6], di at [bp+8].
; CHECK-LABEL: _msc:
; CHECK:       movw %ax, %ds
; CHECK-NEXT:  movw 8(%bp), %ax
; CHECK-NEXT:  movw %ax, _g
define x86_intrcc void @msc(i16 %es, i16 %ds, i16 %di) addrspace(5) {
  store i16 %di, ptr @g
  ret void
}

; Locals go below the saved registers; a call through a pointer to an
; interrupt function is PUSHF and a far call.
; CHECK-LABEL: _chain:
; CHECK:       movw %ax, %ds
; CHECK-NEXT:  subw $4, %sp
; CHECK:       pushfw
; CHECK-NEXT:  lcallw *-4(%bp)
; CHECK-NEXT:  movw %bp, %sp
; CHECK-NEXT:  popw %ax
; CHECK:       iretw
define x86_intrcc void @chain() addrspace(5) {
  %p = load ptr addrspace(5), ptr @old
  call x86_intrcc addrspace(5) void %p()
  ret void
}

; A direct call: PUSHF and CALL FAR.
; CHECK-LABEL: _direct:
; CHECK:       pushfw
; CHECK-NEXT:  lcallw $_chain@SEG, $_chain
define void @direct() {
  call x86_intrcc addrspace(5) void @chain()
  ret void
}

; In tiny, the far address of a function has CS as its segment, since a
; .COM file has no segment fixups.
; TINY-LABEL: _vector:
; TINY:       movw %cs, %dx
; TINY:       movw $_regs, %ax
define ptr addrspace(5) @vector() {
  ret ptr addrspace(5) @regs
}
