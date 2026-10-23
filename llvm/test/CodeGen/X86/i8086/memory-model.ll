; The i8086 memory model is the target ABI name. Unknown names are rejected.
; msdos defaults to OMF objects; an ELF triple can still be used.

; RUN: not llc -mtriple=i8086-unknown-unknown -target-abi=giant < %s 2>&1 \
; RUN:   | FileCheck %s --check-prefix=BADMODEL
; BADMODEL: unknown i8086 memory model 'giant'; expected tiny, small, medium, compact, large or huge

; RUN: llc -mtriple=i8086-pc-msdos -filetype=obj < %s -o /dev/null
; RUN: llc -mtriple=i8086-pc-msdos -target-abi=tiny -filetype=obj < %s -o /dev/null

; RUN: llc -mtriple=i8086-unknown-unknown -target-abi=tiny < %s -o /dev/null
; RUN: llc -mtriple=i8086-unknown-unknown -target-abi=small < %s -o /dev/null
; RUN: llc -mtriple=i8086-pc-msdos-elf < %s -o /dev/null

define void @f() {
  ret void
}
