// __near and __far functions: address space 4 (near code) or 5 (far code),
// whatever the memory model's default. A qualifier of the model's own kind
// changes nothing.
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi small -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,NEARCODE
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi medium -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,FARCODE
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi large -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,FARCODE

// CHECK-DAG: @pf = global ptr addrspace(5) @f
// CHECK-DAG: @pn = global ptr addrspace(4) @n
// NEARCODE-DAG: @pd = global ptr addrspace(4) @d
// FARCODE-DAG: @pd = global ptr addrspace(5) @d
int __far f(int x) { return x; }
int __near n(int x) { return x; }
int d(int x) { return x; }
int (__far *pf)(int) = f;
int (__near *pn)(int) = n;
int (*pd)(int) = d;

// CHECK-LABEL: define{{.*}} i16 @f(i16 noundef %x) addrspace(5)
// CHECK-LABEL: define{{.*}} i16 @n(i16 noundef %x) addrspace(4)
// NEARCODE-LABEL: define{{.*}} i16 @d(i16 noundef %x) addrspace(4)
// FARCODE-LABEL: define{{.*}} i16 @d(i16 noundef %x) addrspace(5)

// CHECK-LABEL: define{{.*}} @call(
// CHECK: call addrspace(5) i16 @f(
// CHECK: call addrspace(4) i16 @n(
// CHECK: call addrspace(5) i16 %{{.*}}(
int call(void) { return f(1) + n(2) + pf(3); }
