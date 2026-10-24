; The 16-bit Windows entry sequence ("i8086-windows-prologue", far functions
; only) and __loadds ("i8086-loadds"). DS is saved at [bp-2], below the
; saved BP and above the callee-saved registers.
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=large < %s | FileCheck %s
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=large -filetype=obj %s -o %t.obj
; RUN: %python %S/../../../MC/X86/i8086/Inputs/omfdump.py %t.obj \
; RUN:   | FileCheck %s --check-prefix=OBJ
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=tiny < %s | FileCheck %s --check-prefix=TINY

; CHECK-LABEL: _win:
; CHECK:       # %bb.0:
; CHECK-NEXT:  pushw %ds
; CHECK-NEXT:  popw %ax
; CHECK-NEXT:  nop
; CHECK-NEXT:  incw %bp
; CHECK-NEXT:  pushw %bp
; CHECK-NEXT:  movw %sp, %bp
; CHECK-NEXT:  pushw %ds
; CHECK-NEXT:  movw %ax, %ds
; CHECK-NEXT:  pushw %si
; CHECK-NEXT:  pushw %ax
; CHECK-NEXT:  movw $7, -6(%bp)
; CHECK:       lcallw $_ext@SEG, $_ext
; CHECK:       leaw -4(%bp), %sp
; CHECK-NEXT:  popw %si
; CHECK-NEXT:  popw %ds
; CHECK-NEXT:  popw %bp
; CHECK-NEXT:  decw %bp
; CHECK-NEXT:  lretw
define i16 @win(i16 %a) addrspace(5) "i8086-windows-prologue" {
  %x = alloca i16
  store i16 7, ptr %x
  call addrspace(5) void @ext(ptr %x)
  call void asm sideeffect "", "~{si}"()
  %v = load i16, ptr %x
  %r = add i16 %v, %a
  ret i16 %r
}

; In a DLL the loader writes "mov ax, DGROUP" over the first 3 bytes;
; __loadds has it there already.
; CHECK-LABEL: _win_loadds:
; CHECK:       # %bb.0:
; CHECK-NEXT:  movw $DGROUP@SEG, %ax
; CHECK-NEXT:  incw %bp
; CHECK-NEXT:  pushw %bp
; CHECK-NEXT:  movw %sp, %bp
; CHECK-NEXT:  pushw %ds
; CHECK-NEXT:  movw %ax, %ds
; CHECK-NEXT:  movw 6(%bp), %ax
; CHECK-NEXT:  popw %ds
; CHECK-NEXT:  popw %bp
; CHECK-NEXT:  decw %bp
; CHECK-NEXT:  lretw
define i16 @win_loadds(i16 %a) addrspace(5) "i8086-windows-prologue" "i8086-loadds" {
  ret i16 %a
}

; CHECK-LABEL: {{^}}_loadds:
; CHECK:       # %bb.0:
; CHECK-NEXT:  pushw %bp
; CHECK-NEXT:  movw %sp, %bp
; CHECK-NEXT:  pushw %ds
; CHECK-NEXT:  movw $DGROUP@SEG, %ax
; CHECK-NEXT:  movw %ax, %ds
; CHECK-NEXT:  movw 4(%bp), %ax
; CHECK-NEXT:  popw %ds
; CHECK-NEXT:  popw %bp
; CHECK-NEXT:  retw
; In the tiny model DGROUP is the code segment.
; TINY-LABEL:  {{^}}_loadds:
; TINY:        pushw %ds
; TINY-NEXT:   movw %cs, %ax
; TINY-NEXT:   movw %ax, %ds
define i16 @loadds(i16 %a) addrspace(4) "i8086-loadds" {
  ret i16 %a
}

; Near functions cannot be called from Windows: no Windows sequence.
; CHECK-LABEL: _near:
; CHECK:       # %bb.0:
; CHECK-NEXT:  pushw %bp
; CHECK-NEXT:  movw %sp, %bp
; CHECK-NEXT:  movw 4(%bp), %ax
; CHECK-NEXT:  popw %bp
; CHECK-NEXT:  retw
define i16 @near(i16 %a) addrspace(4) "i8086-windows-prologue" {
  ret i16 %a
}

declare void @ext(ptr) addrspace(5)

; The bytes Windows looks for and patches, and the DGROUP group fixups.
; OBJ:      PUBDEF '_win' group=0 seg=1 off=0000
; OBJ:      LEDATA seg=1 off=0000 len={{[0-9]+}} 1e5890455589e51e8ed856
; OBJ:      FIXUP @{{[0-9A-F]+}} seg base16 frame=F1:1 target=T5:1
; OBJ:      FIXUP @{{[0-9A-F]+}} seg base16 frame=F1:1 target=T5:1
; OBJ-NOT:  EXTDEF 'DGROUP'
