// RUN: %clang_cc1 -triple m68k-unknown-amigaos -E -dM %s -o %t
// RUN: FileCheck %s < %t
// RUN: FileCheck %s --check-prefix=NOT < %t

// AmigaOS predefines what gcc for AmigaOS and vbcc do, and the Amiga
// compiler keywords used in the NDK headers.

// CHECK-DAG: #define __amigaos__ 1
// CHECK-DAG: #define __amiga__ 1
// CHECK-DAG: #define AMIGA 1
// CHECK-DAG: #define MCH_AMIGA 1
// CHECK-DAG: #define __AMIGA__ 1
// CHECK-DAG: #define __m68k__ 1
// CHECK-DAG: #define mc68000 1
// CHECK-DAG: #define __chip __attribute__((__section__(".datachip")))
// CHECK-DAG: #define __far {{$}}
// CHECK-DAG: #define __saveds {{$}}
// CHECK-DAG: #define __stdargs {{$}}
// CHECK-DAG: #define __SIZEOF_LONG_DOUBLE__ 8
// CHECK-DAG: #define __SIZEOF_POINTER__ 4
// CHECK-DAG: #define __SIZEOF_INT__ 4

// NOT-NOT: #define __linux__
// NOT-NOT: #define __unix__
// <sys/cdefs.h> defines __aligned(x), so SAS/C's __aligned is not predefined.
// NOT-NOT: #define __aligned
