// Driver defaults for Atari TOS on 68k.

// RUN: %clang -### -c --target=m68k-atari-tos %s 2>&1 | FileCheck %s
// CHECK: "-cc1" "-triple" "m68k-atari-tos"
// CHECK-SAME: "-mrelocation-model" "static"
// CHECK-SAME: "-internal-isystem" "{{[^"]*}}/include"
// CHECK-SAME: "-internal-isystem" "{{[^"]*}}{{/|\\\\}}..{{/|\\\\}}m68k-atari-tos{{/|\\\\}}include"
// CHECK-NOT: "/usr/include"

// RUN: %clang -### -c --target=m68k-atari-tos --sysroot=%S/no-such %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SYSROOT
// SYSROOT: "-internal-isystem" "{{[^"]*}}no-such{{/|\\\\}}include"

// Linking runs ld.lld to an ELF file that keeps its relocations, with the
// startup code first, then llvm-elf2tos to write the PRG file. Without the
// sysroot's linker script, the C library or libgem, none is passed.
// RUN: %clang -### --target=m68k-atari-tos --sysroot=%S/no-such %s -o hello.prg \
// RUN:   -resource-dir=%S/Inputs/resource_dir 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LINK
// LINK: "{{[^"]*}}ld.lld{{(.exe)?}}" "-m" "m68kelf" "-static" "--emit-relocs"
// LINK-SAME: "--gc-sections" "-e" "_start" "-o" "[[ELF:[^"]*\.elf]]" "{{[^"]*}}crt0.o"
// LINK-NOT: "-T"
// LINK-SAME: "-L{{[^"]*}}no-such{{/|\\\\}}lib" "{{[^"]*}}.o"
// LINK-SAME: "--start-group" "-lc" "--end-group"
// LINK-NEXT: "{{[^"]*}}llvm-elf2tos{{(.exe)?}}" "[[ELF]]" "-o" "hello.prg" "--symbols"

// With the sysroot of the clang-dos package: tos.ld, libgem.a and the
// builtins.
// RUN: %clang -### --target=m68k-atari-tos --sysroot=%S/Inputs/atari_tos_tree \
// RUN:   -resource-dir=%S/Inputs/resource_dir %s -o hello.prg 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TREE
// TREE: "-T" "{{[^"]*}}atari_tos_tree{{/|\\\\}}lib{{/|\\\\}}tos.ld"
// TREE-SAME: "{{[^"]*}}atari_tos_tree{{/|\\\\}}lib{{/|\\\\}}crt0.o"
// TREE-SAME: "--start-group" "-lc" "-lgem"
// TREE-SAME: "{{[^"]*}}atari_tos_tree{{/|\\\\}}lib{{/|\\\\}}libclang_rt.builtins.a"
// TREE-SAME: "--end-group"

// A linker script of the user's own replaces tos.ld.
// RUN: %clang -### --target=m68k-atari-tos --sysroot=%S/Inputs/atari_tos_tree \
// RUN:   -T my.ld %s -o hello.prg 2>&1 | FileCheck %s --check-prefix=SCRIPT
// SCRIPT-NOT: tos.ld
// SCRIPT: "-T" "my.ld"
// SCRIPT-NOT: tos.ld

// RUN: %clang -### --target=m68k-atari-tos --sysroot=%S/no-such -nostdlib -s \
// RUN:   %s -o hello.prg 2>&1 | FileCheck %s --check-prefix=NOSTDLIB
// NOSTDLIB: "{{[^"]*}}ld.lld{{(.exe)?}}"
// NOSTDLIB-NOT: crt0.o
// NOSTDLIB-NOT: "-lc"
// NOSTDLIB: "{{[^"]*}}llvm-elf2tos{{(.exe)?}}" "{{[^"]*}}.elf" "-o" "hello.prg"{{$}}

// -r stops at a relocatable ELF file.
// RUN: %clang -### --target=m68k-atari-tos --sysroot=%S/no-such -r %s \
// RUN:   -o part.o 2>&1 | FileCheck %s --check-prefix=RELOC
// RELOC: "{{[^"]*}}ld.lld{{(.exe)?}}" "-m" "m68kelf" "-r" "-o" "part.o"
// RELOC-NOT: llvm-elf2tos
