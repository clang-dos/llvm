// Before the 80386 there is no Jcc rel16, so an out-of-range conditional
// branch is an error instead of being relaxed to a 386 instruction.

// RUN: not llvm-mc -triple i8086-unknown-unknown -filetype=obj -o /dev/null %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ERR
// RUN: llvm-mc -triple i8086-unknown-unknown -mcpu=i386 -filetype=obj -o %t %s
// RUN: llvm-objdump -d --triple=i8086 %t | FileCheck %s --check-prefix=I386

// Unconditional jumps relax to JMP rel16, which the 8086 has.
// RUN: llvm-mc -triple i8086-unknown-unknown -filetype=obj --defsym=JMPONLY=1 -o %t.jmp %s
// RUN: llvm-objdump -d --triple=i8086 %t.jmp | FileCheck %s --check-prefix=JMP

.ifdef JMPONLY
  jmp far_away
.else
// ERR: error: value of 200 is too large for field of 1 byte
  jne far_away
.endif
// I386: 0f 85 c8 00 {{.*}}jne
// JMP: e9 c8 00 {{.*}}jmp
  .fill 200, 1, 0x90
far_away:
  ret
