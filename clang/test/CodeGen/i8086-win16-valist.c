// On 16-bit Windows a va_list is a far pointer in every memory model: in a
// DLL the arguments are on the caller's stack, outside DGROUP.
// RUN: %clang_cc1 -triple i8086-pc-win16 -target-abi small -emit-llvm %s -o - \
// RUN:   | FileCheck %s --check-prefix=WIN
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi small -emit-llvm %s -o - \
// RUN:   | FileCheck %s --check-prefix=DOS

#include <stdarg.h>

#ifdef __WINDOWS__
_Static_assert(sizeof(va_list) == 4, "far va_list");
#else
_Static_assert(sizeof(va_list) == 2, "near va_list");
#endif

// WIN-LABEL: define{{.*}} i16 @first(
// WIN:       %ap = alloca ptr addrspace(2)
// WIN:       va_arg ptr %ap, i16
// DOS-LABEL: define{{.*}} i16 @first(
// DOS:       %ap = alloca ptr,
int first(int n, ...) {
  va_list ap;
  va_start(ap, n);
  int v = va_arg(ap, int);
  va_end(ap);
  return v;
}
