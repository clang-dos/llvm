// RUN: %clang_cc1 -triple m68k-atari-tos -E -dM %s -o %t
// RUN: FileCheck %s < %t
// RUN: FileCheck %s --check-prefix=NOT < %t

// Atari TOS predefines what gcc for the Atari does, less __MINT__, and
// Pure C's __TOS__ and vbcc's __ATARI__.

// CHECK-DAG: #define __atarist__ 1
// CHECK-DAG: #define __atarist 1
// CHECK-DAG: #define atarist 1
// CHECK-DAG: #define __TOS__ 1
// CHECK-DAG: #define __ATARI__ 1
// CHECK-DAG: #define __m68k__ 1
// CHECK-DAG: #define mc68000 1
// CHECK-DAG: #define __SIZEOF_LONG_DOUBLE__ 8
// CHECK-DAG: #define __SIZEOF_POINTER__ 4
// CHECK-DAG: #define __SIZEOF_INT__ 4

// NOT-NOT: #define __MINT__
// NOT-NOT: #define __linux__
// NOT-NOT: #define __unix__
// NOT-NOT: #define __amigaos__
