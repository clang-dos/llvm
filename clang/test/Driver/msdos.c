// The i8086-pc-msdos toolchain: -mcmodel= is the memory model, objects are
// OMF, and programs are linked with a Watcom-style linker (wlink or JWlink).

// RUN: %clang --target=i8086-pc-msdos --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -### %s 2>&1 | FileCheck %s --check-prefix=SMALL
// SMALL:      "-cc1" "-triple" "i8086-pc-msdos"
// SMALL-SAME: "-target-abi" "small"
// SMALL-SAME: "-internal-isystem" "{{.*}}msdos_tree{{/|\\\\}}include"
// SMALL:      "{{.*}}wlink" "format" "dos" "option" "quiet" "name" "a.exe"
// SMALL-SAME: "file" "{{.*}}msdos_tree{{/|\\\\}}lib{{/|\\\\}}small{{/|\\\\}}crt0.obj"
// SMALL-SAME: "file" "{{.*}}.o"
// SMALL-SAME: "library" "{{.*}}msdos_tree{{/|\\\\}}lib{{/|\\\\}}small{{/|\\\\}}libc.lib"
// SMALL-NOT:  "com"
// SMALL-NOT:  "-mcmodel

// The tiny model makes a .COM file.
// RUN: %clang --target=i8086-pc-msdos --sysroot=%S/Inputs/msdos_tree \
// RUN:   -mcmodel=tiny -fuse-ld=wlink -### %s -o hello.com 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TINY
// TINY:      "-target-abi" "tiny"
// TINY:      "format" "dos" "com" "option" "quiet" "disable" "1087" "name" "hello.com"
// TINY-SAME: "file" "{{.*}}lib{{/|\\\\}}tiny{{/|\\\\}}crt0.obj"

// The assembler gets the memory model too.
// RUN: %clang --target=i8086-pc-msdos -mcmodel=tiny -### -c -x assembler %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=AS
// AS: "-cc1as" {{.*}}"-target-abi" "tiny"

// -l, -L, -Wl, -nostdlib and -nostartfiles.
// RUN: %clang --target=i8086-pc-msdos --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=jwlink -L/some/dir -lfoo -lbar -Wl,option,map=a.map -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LIBS
// LIBS:      "{{.*}}jwlink" {{.*}}"libpath" "/some/dir"
// LIBS-SAME: "library" "{{.*}}lib{{/|\\\\}}small{{/|\\\\}}libfoo.a" "library" "bar"
// LIBS-SAME: "option" "map=a.map"
// RUN: %clang --target=i8086-pc-msdos --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -nostdlib -### %s 2>&1 | FileCheck %s --check-prefix=NOSTD
// NOSTD-NOT: crt0.obj
// NOSTD-NOT: libc.lib

// Unknown memory models are rejected.
// RUN: not %clang --target=i8086-pc-msdos -mcmodel=giant -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=BAD
// BAD: error: unsupported argument 'giant' to option '-mcmodel=' for target 'i8086'

int main(void) { return 0; }
