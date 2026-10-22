; The i8086 memory model is the target ABI name. Unknown names are rejected,
; and the OMF object format reports that it is not supported yet.

; RUN: not llc -mtriple=i8086-unknown-unknown -target-abi=giant < %s 2>&1 \
; RUN:   | FileCheck %s --check-prefix=BADMODEL
; BADMODEL: unknown i8086 memory model 'giant'; expected tiny, small, medium, compact, large or huge

; RUN: not llc -mtriple=i8086-pc-msdos < %s 2>&1 | FileCheck %s --check-prefix=OMF
; OMF: the OMF object file format is not supported yet; use an ELF triple such as i8086-pc-msdos-elf

; RUN: llc -mtriple=i8086-unknown-unknown -target-abi=tiny < %s -o /dev/null
; RUN: llc -mtriple=i8086-unknown-unknown -target-abi=small < %s -o /dev/null
; RUN: llc -mtriple=i8086-pc-msdos-elf < %s -o /dev/null

define void @f() {
  ret void
}
