// The i8086 (16-bit real mode) target: int and near pointers are 16 bits,
// long is 32 bits, long double is double, and everything is 2-byte aligned.
// Functions are in the near code address space (4).
// RUN: %clang_cc1 -triple i8086-unknown-unknown -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i8086-unknown-unknown -target-abi tiny -emit-llvm -o - %s | FileCheck %s
// RUN: not %clang_cc1 -triple i8086-unknown-unknown -target-abi enormous -emit-llvm -o - %s 2>&1 | FileCheck %s --check-prefix=BAD

// CHECK: target datalayout = "e-m:e-p:16:16-p1:16:16-p2:32:16:16:16-pu3:32:16:16:32-p4:16:16-p5:32:16:16:16-p256:16:16-p257:16:16-p258:16:16-i32:16-i64:16-i128:16-f32:16-f64:16-f80:16-n8:16-a:0:16-S16-P4"
// BAD: error: unknown target ABI 'enormous'

_Static_assert(sizeof(short) == 2, "");
_Static_assert(sizeof(int) == 2, "");
_Static_assert(sizeof(long) == 4, "");
_Static_assert(sizeof(long long) == 8, "");
_Static_assert(sizeof(void *) == 2, "");
_Static_assert(sizeof(void (*)(void)) == 2, "");
_Static_assert(sizeof(__SIZE_TYPE__) == 2, "");
_Static_assert(sizeof(__PTRDIFF_TYPE__) == 2, "");
_Static_assert(sizeof(__WCHAR_TYPE__) == 2, "");
_Static_assert(sizeof(long double) == 8, "");
_Static_assert(_Alignof(long) == 2, "");
_Static_assert(_Alignof(long long) == 2, "");
_Static_assert(_Alignof(double) == 2, "");

struct S { char c; long l; };
_Static_assert(sizeof(struct S) == 6, "");

void g(void);

// CHECK: @fp = global ptr addrspace(4) @g, align 2
void (*fp)(void) = g;

// CHECK: define dso_local i32 @f(i16 noundef %i, i32 noundef %l, ptr noundef %p) addrspace(4)
long f(int i, long l, char *p) {
  // Locals of 8-byte types are only 2-byte aligned.
  // CHECK: alloca i64, align 2
  long long ll = l;
  return i + ll + *p;
}
