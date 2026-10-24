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
