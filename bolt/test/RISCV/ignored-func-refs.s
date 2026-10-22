## Reuse one input to test ignored references, transitive entry patching, and
## moving linker symbols. source retains middle, which retains leaf and target.
# RUN: llvm-mc -triple=riscv64 -filetype=obj %s -o %t.o
# RUN: ld.lld --emit-relocs --no-relax -Ttext=0x10000 -e source %t.o -o %t.exe

# RUN: llvm-bolt %t.exe --skip-funcs=source --print-cfg --print-only=end_refs -o %t.skip | FileCheck %s --check-prefix=END
# RUN: llvm-objdump --disassemble-symbols=leaf,middle,target,source %t.skip | FileCheck %s --check-prefix=KEEP
# RUN: llvm-bolt %t.exe --force-patch -o %t.patch 2>&1 | FileCheck %s --check-prefix=PATCH
# RUN: llvm-objdump --disassemble-symbols=leaf,middle,target,source %t.patch | FileCheck %s --check-prefix=KEEP
# RUN: not llvm-bolt %t.exe --skip-funcs=end_refs -o %t.end 2>&1 | FileCheck %s --check-prefix=ERROR

## Check original addresses and instructions, so an entry trampoline cannot
## hide a moved target or overwrite a function retained later in the pass.
# KEEP-LABEL: 0000000000010000 <leaf>:
# KEEP-NEXT: {{.*}} nop
# KEEP-LABEL: 000000000001000c <middle>:
# KEEP-NEXT: {{.*}} auipc ra,
# KEEP-LABEL: 0000000000010038 <target>:
# KEEP-LABEL: 0000000000010044 <source>:
# PATCH: BOLT-WARNING: failed to patch entries in source

## _end coincides with end_refs' end, but must keep its own symbol identity.
# END: auipc a1, %pcrel_hi(_end)
# ERROR: BOLT-ERROR: cannot update RISC-V instruction reference to _end at address 0x{{[0-9a-f]+}} in ignored function end_refs

  .option norvc
  .text
  .globl leaf, middle, target, source, end_refs
  .type leaf, @function
leaf:
  nop
  nop
  jr t0
  .size leaf, .-leaf

  .type middle, @function
middle:
  call leaf
  lla a1, leaf+4
  lui a1, %hi(leaf+8)
  addi a1, a1, %lo(leaf+8)
  beq a0, a1, leaf
## No relocation describes this pair; JALR clears bit zero to reach target+4.
  auipc t0, 0
  jalr t0, 17(t0)
  ret
  .size middle, .-middle

  .type target, @function
target:
  nop
  nop
  jr t0
  .size target, .-target

## Eight bytes cannot hold an entry trampoline. Visit source after its targets
## to exercise cancelling their patches when source is retained.
  .type source, @function
source:
  jal t0, middle
  ret
  .size source, .-source

  .type end_refs, @function
end_refs:
  lla a1, _end
  lui a1, %hi(_end)
  addi a1, a1, %lo(_end)
  ret
  .size end_refs, .-end_refs
