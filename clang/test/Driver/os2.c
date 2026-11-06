// The OS/2 toolchains. 16-bit OS/2 1.x (i8086-pc-os2): NE programs, linked
// as the DOS and Windows ones are, with the libraries in
// <sysroot>/lib/os2/<model>. 32-bit OS/2 2.x and later (i386-pc-os2): ELF
// objects linked into LX programs, with the sysroot in i386-os2.

// RUN: %clang --target=i8086-pc-os2 --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -### %s 2>&1 | FileCheck %s --check-prefix=NE
// NE:      "-cc1" "-triple" "i8086-pc-os2"
// NE-SAME: "-target-abi" "small"
// NE-NOT:  "-mwindows-prologue
// NE:      "{{.*}}wlink" "format" "os2" "pmcompatible" "option" "quiet" "option" "eliminate" "name" "a.exe"
// NE-SAME: "option" "heapsize=4096" "option" "stack=8192" "option" "protmode"
// NE-SAME: "file" "{{.*}}msdos_tree{{/|\\\\}}lib{{/|\\\\}}os2{{/|\\\\}}small{{/|\\\\}}crt0.obj"
// NE-SAME: "library" "{{.*}}msdos_tree{{/|\\\\}}lib{{/|\\\\}}os2{{/|\\\\}}small{{/|\\\\}}libc.lib"
// NE-SAME: "@{{.*}}msdos_tree{{/|\\\\}}lib{{/|\\\\}}os2{{/|\\\\}}os2.lnk"

// A Presentation Manager program.
// RUN: %clang --target=i8086-pc-os2 --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -mwindows -mcmodel=large -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NEPM
// NEPM: "format" "os2" "pm"
// NEPM-SAME: "file" "{{.*}}os2{{/|\\\\}}large{{/|\\\\}}crt0.obj"

// Protected mode has no segment arithmetic, so no tiny or huge model.
// RUN: not %clang --target=i8086-pc-os2 -mcmodel=huge -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=HUGE
// HUGE: error: unsupported argument 'huge' to option '-mcmodel=' for target 'i8086-pc-os2'

// C++ has no exceptions or RTTI by default on 16-bit OS/2.
// RUN: %clangxx --target=i8086-pc-os2 -### -c -x c++ %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CXX16
// CXX16: "-cc1"
// CXX16-NOT: "-fcxx-exceptions"
// CXX16-SAME: "-fno-rtti"

// RUN: %clang --target=i386-pc-os2 --sysroot=%S/Inputs/os2_tree \
// RUN:   -fuse-ld=wlink -### %s 2>&1 | FileCheck %s --check-prefix=LX
// LX:      "-cc1" "-triple" "i386-pc-os2"
// LX-SAME: "-target-cpu" "i386"
// LX-SAME: "-internal-isystem" "{{.*}}os2_tree{{/|\\\\}}include"
// LX:      "{{.*}}wlink" "format" "os2" "lx" "pmcompatible" "option" "quiet" "option" "eliminate" "name" "a.exe"
// LX-SAME: "option" "stack=65536"
// LX-SAME: "file" "{{.*}}os2_tree{{/|\\\\}}lib{{/|\\\\}}crt0.o" "option" "start=_start"
// LX-SAME: "library" "{{.*}}os2_tree{{/|\\\\}}lib{{/|\\\\}}libc.a"
// LX-SAME: "library" "{{.*}}os2_tree{{/|\\\\}}lib{{/|\\\\}}libclang_rt.builtins.a"
// LX-SAME: "@{{.*}}os2_tree{{/|\\\\}}lib{{/|\\\\}}os2.lnk"

// RUN: %clang --target=i386-pc-os2 --sysroot=%S/Inputs/os2_tree \
// RUN:   -fuse-ld=wlink -mwindows -### %s 2>&1 | FileCheck %s --check-prefix=LXPM
// LXPM: "format" "os2" "lx" "pm"

// A DLL.
// RUN: %clang --target=i386-pc-os2 --sysroot=%S/Inputs/os2_tree \
// RUN:   -fuse-ld=wlink -shared -### %s -o MYLIB.DLL 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LXDLL
// LXDLL: "format" "os2" "lx" "dll" "initinstance" "terminstance"
// LXDLL-NOT: "stack=
// LXDLL-SAME: "segment" "type" "data" "nonshared"
// LXDLL-SAME: "file" "{{.*}}dllcrt0.o" "option" "start=_dll_start"

// Code is not position-independent.
// RUN: %clang --target=i386-pc-os2 -fPIC -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NOPIC
// NOPIC-NOT: "-pic-level"

// The CPU of the arch name, without SSE.
// RUN: %clang --target=i686-pc-os2 -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=I686
// I686: "-target-cpu" "pentiumpro"
