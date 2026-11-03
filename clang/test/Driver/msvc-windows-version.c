// A Windows version in an MSVC triple sets the default CPU and, with LLD, the
// subsystem and OS version that the linker writes, as for MinGW.

// Windows NT 3.51: also a writable .rdata for the NT 3.x loader.
// RUN: %clang -### --target=i386-pc-windows3.51-msvc -fuse-ld=lld %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=CPU-I386,V3_51 %s
// Windows 95 and NT 4.0
// RUN: %clang -### --target=i486-pc-windows4.0-msvc -fuse-ld=lld %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=CPU-I486,V4_0 %s
// Windows XP
// RUN: %clang -### --target=i586-pc-windows5.1-msvc -fuse-ld=lld %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=CPU-PENTIUM,V5_1 %s

// CPU-I386: "-target-cpu" "i386"
// CPU-I486: "-target-cpu" "i486"
// CPU-PENTIUM: "-target-cpu" "pentium"

// V3_51: "-subsystem:default,3.51" "-osversion:3.51" "-section:.rdata,RW"
// V4_0: "-subsystem:default,4.0" "-osversion:4.0"
// V4_0-NOT: .rdata,RW
// V5_1: "-subsystem:default,5.1" "-osversion:5.1"
// V5_1-NOT: .rdata,RW

// A /subsystem given with /link comes later and wins.
// RUN: %clang -### --target=i386-pc-windows3.10-msvc -fuse-ld=lld %s \
// RUN:   -Wl,-subsystem:windows 2>&1 | FileCheck --check-prefix=LINK %s
// LINK: "-subsystem:default,3.10" "-osversion:3.10" "-section:.rdata,RW"
// LINK-SAME: "-subsystem:windows"

// Without a version, or with link.exe, nothing changes.
// RUN: %clang -### --target=i686-pc-windows-msvc -fuse-ld=lld %s 2>&1 \
// RUN:   | FileCheck --check-prefix=NOVERSION %s
// RUN: %clang -### --target=i386-pc-windows3.51-msvc -fuse-ld=link %s 2>&1 \
// RUN:   | FileCheck --check-prefix=NOVERSION %s
// NOVERSION-NOT: -subsystem:default
// NOVERSION-NOT: -osversion
