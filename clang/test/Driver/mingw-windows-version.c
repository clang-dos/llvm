// A Windows version in a MinGW triple sets the default CPU and the subsystem
// and OS version that the linker writes.

// Windows 95 and NT 4.0
// RUN: %clang -### --target=i586-pc-windows4.0-gnu --sysroot= %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=CPU-PENTIUM,V4_0 %s
// Windows 98
// RUN: %clang -### --target=i486-pc-windows4.10-gnu --sysroot= %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=CPU-I486,V4_10 %s
// Windows ME
// RUN: %clang -### --target=i586-pc-windows4.90-gnu --sysroot= %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=CPU-PENTIUM,V4_90 %s
// RUN: %clang -### --target=i386-pc-windows4.0-gnu --sysroot= %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=CPU-I386,V4_0 %s
// Windows XP: no SSE by default either.
// RUN: %clang -### --target=i686-pc-windows5.1-gnu --sysroot= %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=CPU-I686,V5_1 %s

// CPU-I386: "-target-cpu" "i386"
// CPU-I486: "-target-cpu" "i486"
// CPU-PENTIUM: "-target-cpu" "pentium"
// CPU-I686: "-target-cpu" "i686"

// V4_0: "--major-subsystem-version" "4" "--minor-subsystem-version" "0" "--major-os-version" "4" "--minor-os-version" "0"
// V4_10: "--major-subsystem-version" "4" "--minor-subsystem-version" "10" "--major-os-version" "4" "--minor-os-version" "10"
// V4_90: "--major-subsystem-version" "4" "--minor-subsystem-version" "90" "--major-os-version" "4" "--minor-os-version" "90"
// V5_1: "--major-subsystem-version" "5" "--minor-subsystem-version" "1" "--major-os-version" "5" "--minor-os-version" "1"

// Without a version, nothing changes.
// RUN: %clang -### --target=i686-pc-windows-gnu --sysroot= %s 2>&1 \
// RUN:   | FileCheck --check-prefix=NOVERSION %s
// RUN: %clang -### --target=i686-w64-mingw32 --sysroot= %s 2>&1 \
// RUN:   | FileCheck --check-prefix=NOVERSION %s
// NOVERSION: "-target-cpu" "pentium4"
// NOVERSION-NOT: subsystem-version
// NOVERSION-NOT: os-version

// Vista and later keep the usual default CPU.
// RUN: %clang -### --target=i686-pc-windows6.0-gnu --sysroot= %s 2>&1 \
// RUN:   | FileCheck --check-prefix=VISTA %s
// VISTA: "-target-cpu" "pentium4"
// VISTA: "--major-subsystem-version" "6" "--minor-subsystem-version" "0"

// -march and -Wl still win.
// RUN: %clang -### --target=i586-pc-windows4.0-gnu --sysroot= -march=pentium3 \
// RUN:   -Wl,--subsystem,windows:4.10 %s 2>&1 \
// RUN:   | FileCheck --check-prefix=OVERRIDE %s
// OVERRIDE: "-target-cpu" "pentium3"
// OVERRIDE: "--major-subsystem-version" "4"
// OVERRIDE-SAME: "--subsystem" "windows:4.10"

// The triple keeps its version.
// RUN: %clang -### --target=i586-pc-windows4.0-gnu --sysroot= -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=TRIPLE %s
// TRIPLE: "-triple" "i586-pc-windows4.0-gnu"
