// RUN: %clang_cc1 -triple i8086-unknown-unknown -E -dM < /dev/null | FileCheck %s --check-prefixes=CHECK,SMALL --implicit-check-not=__i386__ --implicit-check-not=__MSDOS__
// RUN: %clang_cc1 -triple i8086-unknown-msdos -target-abi tiny -E -dM < /dev/null | FileCheck %s --check-prefixes=CHECK,TINY,DOS --implicit-check-not=__i386__
// RUN: %clang_cc1 -triple i8086-unknown-msdos -target-cpu i286 -E -dM < /dev/null | FileCheck %s --check-prefixes=CHECK,I286

// CHECK-DAG: #define _M_I86 1
// I286-DAG: #define _M_I286 1
// SMALL-DAG: #define _M_I86SM 1
// TINY-DAG: #define _M_I86TM 1
// DOS-DAG: #define __DOS__ 1
// CHECK-DAG: #define __I86__ 1
// CHECK-DAG: #define __INT_MAX__ 32767
// CHECK-DAG: #define __LONG_MAX__ 2147483647L
// CHECK-DAG: #define __USER_LABEL_PREFIX__ _
// DOS-DAG: #define __MSDOS__ 1
// SMALL-DAG: #define __SMALL__ 1
// CHECK-DAG: #define __SIZEOF_POINTER__ 2
// TINY-DAG: #define __TINY__ 1
// CHECK-DAG: #define __far __attribute__((address_space(2)))
// CHECK-DAG: #define __huge __attribute__((address_space(3)))
// CHECK-DAG: #define __near __attribute__((address_space(0)))
// CHECK-DAG: #define _far __attribute__((address_space(2)))

// The MSDOS driver adds the sysroot's include directory; the host's headers
// are never searched.
// RUN: %clang_cc1 -triple i8086-pc-msdos -E -v < /dev/null 2>&1 | FileCheck %s --check-prefix=SEARCH
// SEARCH: #include <...> search starts here:
// SEARCH-NOT: /usr/include
// SEARCH: End of search list.
