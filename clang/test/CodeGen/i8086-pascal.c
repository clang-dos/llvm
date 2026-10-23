// __pascal (and _pascal) is the x86_16_pascalcc convention: arguments left
// to right, removed by the callee, upper-case name. __export is dllexport.
// RUN: %clang_cc1 -triple i8086-pc-msdos -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi large -emit-llvm %s -o - | FileCheck %s --check-prefix=LARGE

// CHECK: @exported_var = dllexport global i16 7
int __export exported_var = 7;

// CHECK-LABEL: define{{.*}} x86_16_pascalcc i16 @add(i16 noundef %a, i16 noundef %b)
// LARGE-LABEL: define{{.*}} x86_16_pascalcc i16 @add(i16 noundef %a, i16 noundef %b) addrspace(5)
int __pascal add(int a, int b) { return a + b; }

// CHECK-LABEL: define dllexport x86_16_pascalcc i16 @wndproc(i16 noundef %m) addrspace(5)
int __export __far _pascal wndproc(int m) { return m; }

// CHECK-LABEL: define{{.*}} i16 @use()
// CHECK: call x86_16_pascalcc addrspace(4) i16 @add(i16 noundef 1, i16 noundef 2)
// CHECK: load ptr addrspace(4), ptr @fp
// CHECK: call x86_16_pascalcc addrspace(4) i16 %{{.*}}(i16 noundef 3, i16 noundef 4)
int (__pascal *fp)(int, int) = add;
int use(void) { return add(1, 2) + fp(3, 4); }
