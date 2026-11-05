// RUN: %clang_cc1 -triple i8086-pc-msdos -emit-llvm %s -o - | FileCheck %s

// An order known only at run time is an int, which is 16 bits here. The
// switches on it compare 32-bit cases, so it is widened first.

int order(void);
int v;

// CHECK-LABEL: @cmpxchg(
// CHECK: [[S:%.*]] = zext i16 %{{.*}} to i32
// CHECK: switch i32 [[S]]
// CHECK: [[F:%.*]] = zext i16 %{{.*}} to i32
// CHECK: switch i32 [[F]]
int cmpxchg(int e, int d) {
  return __atomic_compare_exchange_n(&v, &e, d, 0, order(), order());
}
