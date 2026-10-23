// Before the 80386 there is no Jcc rel16, so an out-of-range conditional
// branch is relaxed to an inverted short Jcc over a near JMP.

// RUN: llvm-mc -triple i8086-unknown-unknown -filetype=obj -o %t %s
// RUN: llvm-objdump -d --triple=i8086 %t | FileCheck %s --check-prefix=I8086
// RUN: llvm-mc -triple i8086-unknown-unknown -mcpu=i386 -filetype=obj -o %t.386 %s
// RUN: llvm-objdump -d --triple=i8086 %t.386 | FileCheck %s --check-prefix=I386

// I8086:      0: 74 03    je 0x5 <near>
// I8086-NEXT: 2: e9 ca 00 jmp 0xcf <far_away>
// I8086:      5: 72 fe    jb 0x5 <near>
// I8086:      cf: 7e 03    jle 0xd4
// I8086-NEXT: d1: e9 2c ff jmp 0x0 <start>
// I8086-NEXT: d4: c3       retw

// I386:       0: 0f 85 ca 00 jne 0xce <far_away>
// I386:       4: 72 fe       jb 0x4 <near>
// I386:       ce: 0f 8f 2e ff jg 0x0 <start>

start:
  jne far_away
near:
  jb near
  .fill 200, 1, 0x90
far_away:
  jg start
  ret
