; RUN: llvm-mc -triple=m68k -mcpu=M68000 -filetype=obj < %s \
; RUN:   | llvm-objdump -dr - | FileCheck %s --check-prefix=M68000
; RUN: llvm-mc -triple=m68k -mcpu=M68020 -filetype=obj < %s \
; RUN:   | llvm-objdump -dr - | FileCheck %s --check-prefix=M68020

; A branch to a symbol the assembler can't resolve relaxes to a 16-bit
; displacement on the 68000, which has no 32-bit one, and to a 32-bit one
; on the 68020.

; M68000: 60 00 00 00 bra
; M68000-NEXT: R_68K_PC16 external
; M68020: 60 ff bra
; M68020: R_68K_PC32 external
	bra	external
