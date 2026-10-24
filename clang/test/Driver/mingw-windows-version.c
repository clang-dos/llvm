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

// Windows NT 3.1, 3.5 and 3.51 (and Win32s, which wants 3.10): LLD also
// makes .rdata, where it puts the import address table, writable.
// RUN: %clang -### --target=i386-pc-windows3.10-gnu --sysroot= -fuse-ld=lld \
// RUN:   %s 2>&1 | FileCheck --check-prefixes=CPU-I386,V3_10,RDATA-RW %s
// RUN: %clang -### --target=i486-pc-windows3.51-gnu --sysroot= -fuse-ld=lld \
// RUN:   %s 2>&1 | FileCheck --check-prefixes=CPU-I486,V3_51,RDATA-RW %s
// V3_10: "--major-subsystem-version" "3" "--minor-subsystem-version" "10" "--major-os-version" "3" "--minor-os-version" "10"
// V3_51: "--major-subsystem-version" "3" "--minor-subsystem-version" "51" "--major-os-version" "3" "--minor-os-version" "51"
// RDATA-RW-SAME: "-Xlink=-section:.rdata,RW"

// Not for Windows 4.0 and later, or for GNU ld, which keeps a writable .idata.
// RUN: %clang -### --target=i486-pc-windows4.0-gnu --sysroot= -fuse-ld=lld \
// RUN:   %s 2>&1 | FileCheck --check-prefix=RDATA-RO %s
// RUN: %clang -### --target=i386-pc-windows3.10-gnu --sysroot= -fuse-ld=ld \
// RUN:   %s 2>&1 | FileCheck --check-prefix=RDATA-RO %s
// RDATA-RO-NOT: -section:.rdata

// Before Vista, native TLS works only in the program and the DLLs loaded
// with it, so versions below 6.0 use emulated TLS by default.
// RUN: %clang -### --target=i586-pc-windows5.1-gnu --sysroot= -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=EMUTLS %s
// RUN: %clang -### --target=i486-pc-windows4.0-gnu --sysroot= -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=EMUTLS %s
// RUN: %clang -### --target=i586-pc-windows5.1-gnu --sysroot= -c \
// RUN:   -fno-emulated-tls %s 2>&1 | FileCheck --check-prefix=NOEMUTLS %s
// RUN: %clang -### --target=i686-pc-windows6.0-gnu --sysroot= -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=NOEMUTLS %s
// RUN: %clang -### --target=i686-w64-mingw32 --sysroot= -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=NOEMUTLS %s
// EMUTLS: "-femulated-tls"
// NOEMUTLS-NOT: "-femulated-tls"
