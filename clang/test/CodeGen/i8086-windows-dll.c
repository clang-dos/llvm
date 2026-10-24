// -mwindows-dll marks functions safestack in the near data models, so that
// the backend keeps locals whose address escapes in DGROUP; the far data
// models need nothing.
// RUN: %clang_cc1 -triple i8086-pc-win16 -target-abi small -mwindows-dll -emit-llvm -o - %s | FileCheck %s --check-prefix=NEAR
// RUN: %clang_cc1 -triple i8086-pc-win16 -target-abi medium -mwindows-dll -emit-llvm -o - %s | FileCheck %s --check-prefix=NEAR
// RUN: %clang_cc1 -triple i8086-pc-win16 -target-abi large -mwindows-dll -emit-llvm -o - %s | FileCheck %s --check-prefix=FAR
// RUN: %clang_cc1 -triple i8086-pc-win16 -target-abi small -emit-llvm -o - %s | FileCheck %s --check-prefix=FAR

void use(char *);
void f(void) {
  char buf[8];
  use(buf);
}
// NEAR: define {{.*}}void @f() {{.*}}#[[A:[0-9]+]]
// NEAR: attributes #[[A]] = {{{.*}}safestack
// FAR: define {{.*}}void @f()
// FAR-NOT: safestack
