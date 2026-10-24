// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi small -fsyntax-only -verify=small %s

// A library builtin keeps the memory model's pointers: a far argument
// converts to them (here to huge), and is not passed to the function as a
// far pointer.

char *strcpy(char *, const char *); // small-note {{passing argument to parameter here}}
char __far *fp;

// CHECK-LABEL: define {{.*}}@f
// CHECK: [[H:%.*]] = addrspacecast ptr addrspace(2) %{{.*}} to ptr addrspace(3)
// CHECK: call addrspace(5) ptr addrspace(3) @strcpy(ptr addrspace(3) noundef [[H]],
// CHECK: declare ptr addrspace(3) @strcpy(ptr addrspace(3) noundef, ptr addrspace(3) noundef)
void f(void) {
  strcpy(fp, "x"); // small-error {{changes address space of pointer}}
}
