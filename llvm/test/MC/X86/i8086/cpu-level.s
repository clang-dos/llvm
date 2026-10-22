// The i8086 triple assembles in 16-bit mode and rejects instructions that are
// newer than the selected CPU.

// RUN: not llvm-mc -triple i8086-unknown-unknown %s 2>&1 \
// RUN:   | FileCheck %s --check-prefixes=ERR8086,ERR186,ERR286 --implicit-check-not=error:
// RUN: not llvm-mc -triple i8086-unknown-unknown -mcpu=i8088 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefixes=ERR8086,ERR186,ERR286 --implicit-check-not=error:
// RUN: not llvm-mc -triple i8086-unknown-unknown -mcpu=i186 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefixes=ERR186,ERR286 --implicit-check-not=error:
// RUN: not llvm-mc -triple i8086-unknown-unknown -mcpu=v30 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefixes=ERR186,ERR286 --implicit-check-not=error:
// RUN: not llvm-mc -triple i8086-unknown-unknown -mcpu=i286 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefixes=ERR286 --implicit-check-not=error:
// RUN: llvm-mc -triple i8086-unknown-unknown -mcpu=i386 -show-encoding %s \
// RUN:   | FileCheck %s --check-prefix=ENC
// RUN: llvm-mc -triple i8086-unknown-unknown -mattr=+386 -show-encoding %s \
// RUN:   | FileCheck %s --check-prefix=ENC

// The i386 triple in 16-bit mode is not restricted.
// RUN: llvm-mc -triple i386-unknown-unknown-code16 -show-encoding %s \
// RUN:   | FileCheck %s --check-prefix=ENC

// 8086 instructions are accepted everywhere. The default mode is 16-bit, so
// there are no operand size prefixes.
// ENC: addw %bx, %ax          # encoding: [0x01,0xd8]
addw %bx, %ax
// ENC: movw 4(%bp,%si), %cx   # encoding: [0x8b,0x4a,0x04]
movw 4(%bp,%si), %cx
// ENC: shlw %ax               # encoding: [0xd1,0xe0]
shlw $1, %ax
// ENC: shlw %cl, %ax          # encoding: [0xd3,0xe0]
shlw %cl, %ax
// ENC: movw %ax, %es          # encoding: [0x8e,0xc0]
movw %ax, %es
// ENC: lcallw *(%bx)          # encoding: [0xff,0x1f]
lcall *(%bx)
// ENC: lretw                  # encoding: [0xcb]
lretw
// ENC: cbtw                   # encoding: [0x98]
cbw
// ENC: int $33                # encoding: [0xcd,0x21]
int $0x21

// 80186 instructions.
// ERR8086: :[[@LINE+1]]:1: error: instruction requires an 80186 or later CPU
pushw $5
// ERR8086: :[[@LINE+1]]:1: error: instruction requires an 80186 or later CPU
shlw $3, %ax
// ERR8086: :[[@LINE+1]]:1: error: instruction requires an 80186 or later CPU
imulw $10, %bx, %ax
// ERR8086: :[[@LINE+1]]:1: error: instruction requires an 80186 or later CPU
enter $4, $0
// ERR8086: :[[@LINE+1]]:1: error: instruction requires an 80186 or later CPU
leave
// ERR8086: :[[@LINE+1]]:1: error: instruction requires an 80186 or later CPU
pushaw
// ERR8086: :[[@LINE+1]]:1: error: instruction requires an 80186 or later CPU
insb %dx, %es:(%di)

// 80286 instructions.
// ERR186: :[[@LINE+1]]:1: error: instruction requires an 80286 or later CPU
lgdtw (%bx)
// ERR186: :[[@LINE+1]]:1: error: instruction requires an 80286 or later CPU
clts

// 80386 instructions.
// ERR286: :[[@LINE+1]]:1: error: instruction requires an 80386 or later CPU
addl %ebx, %eax
// ERR286: :[[@LINE+1]]:1: error: instruction requires an 80386 or later CPU
movw (%eax), %ax
// ERR286: :[[@LINE+1]]:1: error: instruction requires an 80386 or later CPU
movzbw %al, %ax
// ERR286: :[[@LINE+1]]:1: error: instruction requires an 80386 or later CPU
sete %al
// ERR286: :[[@LINE+1]]:1: error: instruction requires an 80386 or later CPU
movw %ax, %fs
// ERR286: :[[@LINE+1]]:1: error: instruction requires an 80386 or later CPU
movw %gs:(%bx), %ax
// ERR286: :[[@LINE+1]]:1: error: instruction requires an 80386 or later CPU
bsfw %ax, %bx
