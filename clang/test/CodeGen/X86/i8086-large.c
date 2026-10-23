// The large memory model: far code and far data. Function pointers are far
// (address space 5) and so are data pointers (address space 2); globals and
// the stack stay in DGROUP (address space 0).
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi large -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi large -dM -E %s | FileCheck %s --check-prefix=MACRO

// MACRO: #define _M_I86LM 1
// MACRO: #define __INTPTR_WIDTH__ 32
// MACRO: #define __LARGE__ 1
// MACRO: #define __SIZE_WIDTH__ 16

_Static_assert(sizeof(int *) == 4, "far data pointer");
_Static_assert(sizeof(int (*)(int *)) == 4, "far function pointer");
_Static_assert(sizeof(__SIZE_TYPE__) == 2, "16-bit size_t");

// CHECK: @fp = global ptr addrspace(5) @deref, align 2
// CHECK: @g = global i16 0, align 2
int g;
int deref(int *p);
int (*fp)(int *) = deref;

// CHECK: define i16 @call(ptr addrspace(2) noundef %p) addrspace(5)
// CHECK: call addrspace(5) i16 %{{.*}}(ptr addrspace(2) noundef %{{.*}})
// CHECK: call addrspace(5) i16 @deref(ptr addrspace(2) noundef addrspacecast (ptr @g to ptr addrspace(2)))
int call(int *p) { return fp(p) + deref(&g); }

// CHECK: !{i32 1, !"target-abi", !"large"}
