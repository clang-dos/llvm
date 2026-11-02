; REQUIRES: m68k
; RUN: llvm-mc -filetype=obj -triple=m68k -o %t.o %s
; RUN: ld.lld -m m68kelf %t.o --defsym=abs16=0x1234 --defsym=abs8=0x56 \
; RUN:   --Ttext=0x10000 --Tdata=0x10100 -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
; RUN: llvm-objdump -s -j .data %t | FileCheck %s --check-prefix=DATA
; RUN: ld.lld -m m68kelf %t.o --defsym=abs16=0x1234 --defsym=abs8=0x56 \
; RUN:   --Ttext=0x10000 --Tdata=0x10100 --emit-relocs -o %t.q
; RUN: llvm-readobj -r %t.q | FileCheck %s --check-prefix=EMIT

;; Static linking for m68k: absolute and PC-relative relocations, and calls
;; through the PLT, which resolve to the function itself.

  .text
  .globl _start
_start:
  jsr foo                 ; R_68K_32
  bsr.w foo               ; R_68K_PC16
  lea (data,%pc), %a0     ; R_68K_PC16
  move.l #data, %d0       ; R_68K_32
  rts
foo:
  rts

;; The PC-relative displacements count from the extension word.
; CHECK:      <_start>:
; CHECK-NEXT:   10000: jsr $10016
; CHECK-NEXT:   10006: bsr.w $e
; CHECK-NEXT:   1000a: lea (244,%pc), %a0
; CHECK-NEXT:   1000e: move.l #65792, %d0
; CHECK:      <foo>:
; CHECK-NEXT:   10016: rts

  .data
data:
  .long foo               ; R_68K_32
  .short abs16            ; R_68K_16
  .byte abs8              ; R_68K_8
  .byte 0
  .long foo - .           ; R_68K_PC32

; DATA:      Contents of section .data:
; DATA-NEXT:  10100 00010016 12345600 ffffff0e

; EMIT: R_68K_32 .text 0x16
; EMIT: R_68K_PC16 .data 0x0

;; A 16-bit PC-relative reference that doesn't reach is an error.
; RUN: not ld.lld -m m68kelf %t.o --defsym=abs16=0x1234 --defsym=abs8=0x56 \
; RUN:   --Ttext=0x10000 --Tdata=0x20000 -o /dev/null 2>&1 | FileCheck %s --check-prefix=RANGE
; RANGE: error: {{.*}}relocation R_68K_PC16 out of range: 65524 is not in [-32768, 32767]
