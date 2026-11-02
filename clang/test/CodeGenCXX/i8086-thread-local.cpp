// In the large and huge models a variable's address is a far pointer, an
// addrspacecast of the global: llvm.threadlocal.address takes the global
// itself.
// RUN: %clang_cc1 -triple i8086-pc-msdos -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi large -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge -emit-llvm -o - %s | FileCheck %s

struct S { int a; char name[13]; };

// CHECK-LABEL: define {{.*}}@_Z5localv(
// CHECK: call {{.*}}ptr @llvm.threadlocal.address.p0(ptr {{.*}}@_ZZ5localvE1s)
// CHECK-NOT: addrspacecast (ptr @_ZZ5localvE1s
int *local() {
  static thread_local S s;
  s.a = 1;
  return &s.a;
}

thread_local int g;

// CHECK-LABEL: define {{.*}}@_Z6globalv(
// CHECK: call {{.*}}ptr @llvm.threadlocal.address.p0(ptr {{.*}}@g)
int global() { return g; }
