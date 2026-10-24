// The i8086-pc-msdos toolchain: -mcmodel= is the memory model, objects are
// OMF, and programs are linked with a Watcom-style linker (wlink or JWlink).

// RUN: %clang --target=i8086-pc-msdos --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -### %s 2>&1 | FileCheck %s --check-prefix=SMALL
// SMALL:      "-cc1" "-triple" "i8086-pc-msdos"
// SMALL-SAME: "-target-abi" "small"
// SMALL-SAME: "-internal-isystem" "{{.*}}msdos_tree{{/|\\\\}}include"
// SMALL:      "{{.*}}wlink" "format" "dos" "option" "quiet" "option" "eliminate" "name" "a.exe"
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
// TINY:      "format" "dos" "com" "option" "quiet" "option" "eliminate" "disable" "1087" "name" "hello.com"
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

// Tuning defaults to the target CPU, not to a modern "generic" x86.
// RUN: %clang --target=i8086-pc-msdos -### -c %s 2>&1 | FileCheck %s --check-prefix=TUNE
// TUNE: "-target-cpu" "i8086"
// TUNE-NOT: "-tune-cpu"

// Unknown memory models are rejected.
// RUN: not %clang --target=i8086-pc-msdos -mcmodel=giant -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=BAD
// BAD: error: unsupported argument 'giant' to option '-mcmodel=' for target 'i8086'

// -msize-t=16 gives the huge model a 16-bit size_t: the ABI and the
// libraries are huge16. Other models have a 16-bit size_t already.
// RUN: %clang --target=i8086-pc-msdos --sysroot=%S/Inputs/msdos_tree \
// RUN:   -mcmodel=huge -msize-t=16 -fuse-ld=wlink -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=HUGE16
// HUGE16: "-target-abi" "huge16"
// HUGE16: "file" "{{.*}}lib{{/|\\\\}}huge16{{/|\\\\}}crt0.obj"
// RUN: %clang --target=i8086-pc-msdos -mcmodel=huge -msize-t=32 -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=HUGE32
// HUGE32: "-target-abi" "huge"
// RUN: %clang --target=i8086-pc-msdos -mcmodel=large -msize-t=16 -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LARGE16
// LARGE16: "-target-abi" "large"
// RUN: not %clang --target=i8086-pc-msdos -mcmodel=large -msize-t=32 -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LARGE32
// LARGE32: error: unsupported argument '32' to option '-msize-t=' for target 'i8086 large model'

int main(void) { return 0; }

// Real-mode code has no GOT or PLT, so -fPIC and -fPIE don't apply.
// RUN: %clang --target=i8086-pc-msdos -fPIC -### -c %s 2>&1 | FileCheck %s --check-prefix=NOPIC
// RUN: %clang --target=i8086-pc-msdos -fPIE -### -c %s 2>&1 | FileCheck %s --check-prefix=NOPIC
// NOPIC: "-cc1"
// NOPIC-SAME: "-mrelocation-model" "static"
// NOPIC-NOT: "-pic-level"

// Archives named on the command line are libraries to the linker.
// RUN: %clang --target=i8086-pc-msdos --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -### %s %S/Inputs/msdos_tree/lib/small/libfoo.a \
// RUN:   %S/Inputs/msdos_tree/lib/small/libc.lib 2>&1 | FileCheck %s --check-prefix=ARCHIVE
// ARCHIVE: "file" "{{.*}}.o" "library" "{{.*}}libfoo.a" "library" "{{.*}}libc.lib"

// C++ has no exceptions or RTTI by default, and uses libc++ and libc++abi.
// RUN: %clangxx --target=i8086-pc-msdos --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -### -x c++ %s 2>&1 | FileCheck %s --check-prefix=CXX
// CXX:      "-cc1"
// CXX-NOT:  "-fcxx-exceptions"
// CXX-SAME: "-internal-isystem" "{{.*}}msdos_tree{{/|\\\\}}include{{/|\\\\}}c++{{/|\\\\}}v1"
// CXX-SAME: "-fno-rtti"
// CXX:      "{{.*}}wlink"
// CXX-SAME: "library" "{{.*}}libc++.a" "library" "{{.*}}libc++abi.a"
// CXX-SAME: "library" "{{.*}}libc.lib"
// RUN: %clangxx --target=i8086-pc-msdos --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -nostdinc++ -nostdlib++ -### -x c++ %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NOCXX
// NOCXX-NOT: c++{{/|\\\\}}v1
// NOCXX-NOT: libc++

// In the far-data models, variables over 32767 bytes are far data by
// default. -mlarge-data-threshold= changes that, and 0 turns it off.
// RUN: %clang --target=i8086-pc-msdos -mcmodel=compact -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=FARDATA
// RUN: %clang --target=i8086-pc-msdos -mcmodel=huge -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=FARDATA
// FARDATA: "-mlarge-data-threshold=32767"
// RUN: %clang --target=i8086-pc-msdos -mcmodel=large -mlarge-data-threshold=1000 -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=FARDATA1000
// FARDATA1000: "-mlarge-data-threshold=1000"
// RUN: %clang --target=i8086-pc-msdos -mcmodel=small -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NEARDATA
// RUN: %clang --target=i8086-pc-msdos -mcmodel=medium -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NEARDATA
// NEARDATA-NOT: -mlarge-data-threshold

// -mwindows-prologue= is passed to cc1, and checked.
// RUN: %clang --target=i8086-pc-msdos -mcmodel=large -mwindows-prologue=exported -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=WINPRO
// WINPRO: "-cc1"{{.*}} "-mwindows-prologue=exported"
// RUN: not %clang --target=i8086-pc-msdos -mwindows-prologue=some -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=WINPRO-BAD
// WINPRO-BAD: error: unsupported argument 'some' to option '-mwindows-prologue='
// RUN: not %clang --target=x86_64-linux-gnu -mwindows-prologue=all -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=WINPRO-X64
// WINPRO-X64: error: unsupported option '-mwindows-prologue=' for target 'x86_64-linux-gnu'
