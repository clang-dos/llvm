// The i8086-pc-win16 toolchain: 16-bit Windows programs and DLLs in the NE
// format, linked with a Watcom-style linker (wlink or JWlink), with the
// libraries in <sysroot>/lib/win16/<model>.

// RUN: %clang --target=i8086-pc-win16 --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -### %s 2>&1 | FileCheck %s --check-prefix=EXE
// EXE:      "-cc1" "-triple" "i8086-pc-win16"
// EXE-SAME: "-target-abi" "small"
// EXE-SAME: "-mwindows-prologue=exported"
// EXE:      "{{.*}}wlink" "format" "windows" "option" "quiet" "option" "eliminate" "name" "a.exe"
// EXE-SAME: "option" "heapsize=4096" "option" "stack=8192"
// EXE-SAME: "file" "{{.*}}msdos_tree{{/|\\\\}}lib{{/|\\\\}}win16{{/|\\\\}}small{{/|\\\\}}crt0.obj"
// EXE-SAME: "library" "{{.*}}msdos_tree{{/|\\\\}}lib{{/|\\\\}}win16{{/|\\\\}}small{{/|\\\\}}libc.lib"
// EXE-SAME: "@{{.*}}msdos_tree{{/|\\\\}}lib{{/|\\\\}}win16{{/|\\\\}}windows.lnk"

// A DLL starts with libentry.obj and has no stack of its own.
// RUN: %clang --target=i8086-pc-win16 --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -shared -mcmodel=large -### %s -o MYLIB.DLL 2>&1 \
// RUN:   | FileCheck %s --check-prefix=DLL
// DLL:      "-target-abi" "large"
// DLL:      "format" "windows" "dll" "option" "quiet" "option" "eliminate" "name" "MYLIB.DLL"
// DLL-SAME: "option" "heapsize=4096"
// DLL-NOT:  "stack=
// DLL-SAME: "file" "{{.*}}lib{{/|\\\\}}win16{{/|\\\\}}large{{/|\\\\}}libentry.obj"

// Protected mode has no segment arithmetic, so no tiny or huge model.
// RUN: not %clang --target=i8086-pc-win16 -mcmodel=huge -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=HUGE
// HUGE: error: unsupported argument 'huge' to option '-mcmodel=' for target 'i8086-pc-win16'
// RUN: not %clang --target=i8086-pc-win16 -mcmodel=tiny -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TINY
// TINY: error: unsupported argument 'tiny' to option '-mcmodel=' for target 'i8086-pc-win16'

// -mwindows-prologue= can be changed.
// RUN: %clang --target=i8086-pc-win16 -mwindows-prologue=all -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ALL
// ALL: "-mwindows-prologue=all"
// ALL-NOT: "-mwindows-prologue=exported"

// C++ has no exceptions or RTTI by default.
// RUN: %clangxx --target=i8086-pc-win16 -### -c -x c++ %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CXX
// CXX: "-cc1"
// CXX-NOT: "-fcxx-exceptions"
// CXX-SAME: "-fno-rtti"

// DLL code in the small and medium models (-shared, or -mwindows-dll with
// -c) keeps escaping locals in DGROUP, and links the libraries in
// <model>/dll.
// RUN: %clang --target=i8086-pc-win16 --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -shared -### %s -o MYLIB.DLL 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NEARDLL
// NEARDLL:      "-cc1" {{.*}}"-mwindows-dll"
// NEARDLL:      "{{.*}}wlink"
// NEARDLL-SAME: "library" "{{.*}}win16{{/|\\\\}}small{{/|\\\\}}dll{{/|\\\\}}libc.lib"
// RUN: %clang --target=i8086-pc-win16 -mwindows-dll -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=MDLL
// MDLL: "-mwindows-dll"
// RUN: %clang --target=i8086-pc-win16 -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NOMDLL
// NOMDLL-NOT: "-mwindows-dll"

// A .res file gives the program its resources.
// RUN: %clang --target=i8086-pc-win16 --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -### %s %S/Inputs/win16_res/app.res 2>&1 \
// RUN:   | FileCheck %s --check-prefix=RES
// RES: "{{.*}}wlink" {{.*}}"option" "resource={{.*}}app.res"
// RES-NOT: "file" "{{.*}}app.res"
// RUN: not %clang --target=i8086-pc-win16 --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -### %s %S/Inputs/win16_res/app.res \
// RUN:   %S/Inputs/win16_res/more.res 2>&1 | FileCheck %s --check-prefix=TWORES
// TWORES: error: a 16-bit Windows program or DLL takes one .res file

// Without a Windows version, the program is for protected mode, as Windows
// 3.1 and later run it: Windows 3.0 refuses to run it in real mode.
// RUN: %clang --target=i8086-pc-win16 --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -### %s 2>&1 | FileCheck %s --check-prefix=PROT
// PROT:     "{{.*}}wlink" "format" "windows"
// PROT-NOT: "version=
// PROT-NOT: "segment"
// PROT-SAME: "option" "protmode"

// Windows 3.1 and later: the expected version in the header.
// RUN: %clang --target=i8086-pc-win163.10 --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -### %s 2>&1 | FileCheck %s --check-prefix=WIN31
// WIN31:      "-cc1" "-triple" "i8086-pc-win163.10"
// WIN31:      "{{.*}}wlink" "format" "windows"
// WIN31-SAME: "option" "version=3.10"
// WIN31-NOT:  "segment"
// WIN31-SAME: "option" "protmode"

// Windows 1.x, 2.x and 3.0 in real mode move segments in memory: the code is
// fixed, and so is the data in the far data models and DLLs.
// RUN: %clang --target=i8086-pc-win162.03 --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -### %s 2>&1 | FileCheck %s --check-prefix=WIN2
// WIN2:      "{{.*}}wlink" "format" "windows"
// WIN2-SAME: "option" "version=2.03" "segment" "type" "code" "fixed"
// WIN2-NOT:  "data"
// WIN2-NOT:  "protmode"
// RUN: %clang --target=i8086-pc-win163.0 --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -mcmodel=large -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=WIN30
// WIN30:      "{{.*}}wlink" "format" "windows"
// WIN30-SAME: "option" "version=3.00" "segment" "type" "code" "fixed" "segment" "type" "data" "fixed"
// WIN30-NOT:  "protmode"
// RUN: %clang --target=i8086-pc-win161.01 --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -shared -mcmodel=medium -### %s -o MYLIB.DLL 2>&1 \
// RUN:   | FileCheck %s --check-prefix=WIN1
// WIN1:      "format" "windows" "dll"
// WIN1-SAME: "option" "version=1.01" "segment" "type" "code" "fixed" "segment" "type" "data" "fixed"
