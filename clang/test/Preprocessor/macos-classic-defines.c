// RUN: %clang_cc1 -triple powerpc-apple-macos9.2 -E -dM %s -o %t.ppc
// RUN: FileCheck %s --check-prefixes=CHECK,PPC < %t.ppc
// RUN: FileCheck %s --check-prefixes=NOT < %t.ppc
// RUN: %clang_cc1 -triple powerpc-apple-macosclassic -E -dM %s -o - \
// RUN:   | FileCheck %s --check-prefixes=CHECK,PPC
// RUN: %clang_cc1 -triple m68k-apple-macos -E -dM %s -o %t.m68k
// RUN: FileCheck %s --check-prefixes=CHECK,M68K < %t.m68k
// RUN: FileCheck %s --check-prefixes=NOT < %t.m68k

// Classic Mac OS (System 7 to Mac OS 9) predefines what MPW, CodeWarrior and
// Retro68 do, and nothing that says Mac OS X, AIX or Unix.

// CHECK-DAG: #define macintosh 1
// CHECK-DAG: #define Macintosh 1
// CHECK-DAG: #define __BIG_ENDIAN__ 1
// CHECK-DAG: #define __SIZEOF_LONG_DOUBLE__ 8
// CHECK-DAG: #define __SIZE_TYPE__ long unsigned int
// CHECK-DAG: #define __PTRDIFF_TYPE__ long int

// PPC-DAG: #define __POWERPC__ 1
// PPC-DAG: #define __ppc__ 1
// PPC-DAG: #define __NATURAL_ALIGNMENT__ 1
// PPC-DAG: #define __WCHAR_TYPE__ int

// CHECK-DAG: #define pascal __attribute__((__pascal__))
// M68K-DAG: #define __m68k__ 1
// M68K-DAG: #define __WCHAR_TYPE__ long int

// NOT-NOT: #define __APPLE__
// NOT-NOT: #define __MACH__
// NOT-NOT: #define _AIX
// NOT-NOT: #define __unix
// NOT-NOT: #define __CHAR_UNSIGNED__
