// RUN: %clang_cc1 -triple powerpc-apple-macosx10.4.0 -E -dM %s -o - \
// RUN:   | FileCheck %s

// CHECK-DAG: #define __APPLE__ 1
// CHECK-DAG: #define __MACH__ 1
// CHECK-DAG: #define __BIG_ENDIAN__ 1
// CHECK-DAG: #define __POWERPC__ 1
// CHECK-DAG: #define __ppc__ 1
// CHECK-DAG: #define __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ 1040
// CHECK-DAG: #define __SIZEOF_LONG_DOUBLE__ 16
