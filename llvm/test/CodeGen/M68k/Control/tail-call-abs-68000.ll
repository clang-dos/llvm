; RUN: llc -mtriple=m68k < %s | FileCheck %s --check-prefix=M68000
; RUN: llc -mtriple=m68k -mcpu=M68020 < %s | FileCheck %s --check-prefix=M68020
; RUN: llc -mtriple=m68k -relocation-model=pic < %s | FileCheck %s --check-prefix=PIC
; RUN: llc -mtriple=m68k -filetype=obj < %s | llvm-readobj -r - \
; RUN:   | FileCheck %s --check-prefix=RELOC

; A tail call to another function is a BRA with a 32-bit displacement on
; the 68020 and later. The 68000 and 68010 have no such BRA (its encoding
; traps there), so they jump to the absolute address, or use a 16-bit BRA
; in position-independent code.

declare void @g(i32)

define void @f(i32 %a) {
; M68000-LABEL: f:
; M68000:         jmp g ; TAILCALL
; M68020-LABEL: f:
; M68020:         bra g ; TAILCALL
; PIC-LABEL: f:
; PIC:            bra g@PLT ; TAILCALL
  tail call void @g(i32 %a)
  ret void
}

; RELOC: R_68K_32 g
