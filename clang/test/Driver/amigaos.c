// Driver defaults for AmigaOS on 68k.

// RUN: %clang -### -c --target=m68k-amigaos %s 2>&1 | FileCheck %s
// CHECK: "-cc1" "-triple" "m68k-unknown-amigaos"
// CHECK-SAME: "-mrelocation-model" "static"
// CHECK-SAME: "-internal-isystem" "{{[^"]*}}/include"
// CHECK-SAME: "-internal-isystem" "{{[^"]*}}{{/|\\\\}}..{{/|\\\\}}m68k-amigaos{{/|\\\\}}include"
// CHECK-NOT: "/usr/include"

// RUN: %clang -### -c --target=m68k-amigaos --sysroot=%S/no-such %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SYSROOT
// SYSROOT: "-internal-isystem" "{{[^"]*}}no-such{{/|\\\\}}include"

// -mcmodel= is accepted on m68k.
// RUN: %clang -### -c --target=m68k-amigaos -mcmodel=small %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CMODEL
// CMODEL: "-mcmodel=small"

// Linking runs ld.lld to an ELF file that keeps its relocations, with the
// startup code first, then llvm-elf2hunk to write the Hunk executable.
// RUN: %clang -### --target=m68k-amigaos --sysroot=%S/no-such %s -o hello \
// RUN:   -resource-dir=%S/Inputs/resource_dir 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LINK
// LINK: "{{[^"]*}}ld.lld{{(.exe)?}}" "-m" "m68kelf" "-static" "--emit-relocs"
// LINK-SAME: "-e" "_start" "-o" "[[ELF:[^"]*\.elf]]" "{{[^"]*}}crt0.o"
// LINK-SAME: "-L{{[^"]*}}no-such{{/|\\\\}}lib" "{{[^"]*}}.o"
// LINK-SAME: "--start-group" "-lc" "--end-group"
// LINK-NEXT: "{{[^"]*}}llvm-elf2hunk{{(.exe)?}}" "[[ELF]]" "-o" "hello" "--symbols"

// With the NDK's OS call stubs (libamiga.a) and the builtins in the
// sysroot, as the clang-dos package has them, both are linked.
// RUN: %clang -### --target=m68k-amigaos --sysroot=%S/Inputs/amigaos_tree \
// RUN:   -resource-dir=%S/Inputs/resource_dir %s -o hello 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TREE
// TREE: "{{[^"]*}}amigaos_tree{{/|\\\\}}lib{{/|\\\\}}crt0.o"
// TREE-SAME: "--start-group" "-lc" "-lamiga"
// TREE-SAME: "{{[^"]*}}amigaos_tree{{/|\\\\}}lib{{/|\\\\}}libclang_rt.builtins.a"
// TREE-SAME: "--end-group"

// RUN: %clang -### --target=m68k-amigaos --sysroot=%S/no-such -nostdlib -s \
// RUN:   %s -o hello 2>&1 | FileCheck %s --check-prefix=NOSTDLIB
// NOSTDLIB: "{{[^"]*}}ld.lld{{(.exe)?}}"
// NOSTDLIB-NOT: crt0.o
// NOSTDLIB-NOT: "-lc"
// NOSTDLIB: "{{[^"]*}}llvm-elf2hunk{{(.exe)?}}" "{{[^"]*}}.elf" "-o" "hello"{{$}}
