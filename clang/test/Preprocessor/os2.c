// RUN: %clang_cc1 -E -dM -triple i8086-pc-os2 -target-abi small %s \
// RUN:   | FileCheck %s --check-prefixes=CHECK,OS216
// RUN: %clang_cc1 -E -dM -triple i386-pc-os2 %s \
// RUN:   | FileCheck %s --check-prefixes=CHECK,OS232
// CHECK-DAG: #define __OS2__ 1
// OS216-DAG: #define __SMALL__ 1
// OS216-NOT: #define __32BIT__
// OS232-DAG: #define __32BIT__ 1
// OS232-DAG: #define __FLAT__ 1
// OS232-DAG: #define __WCHAR_TYPE__ unsigned short
