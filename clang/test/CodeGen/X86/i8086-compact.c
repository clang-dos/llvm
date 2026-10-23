// The compact memory model: near code and far data. Data pointers are 32-bit
// segment:offset values (address space 2); globals and the stack stay in
// DGROUP (address space 0) and are cast to far when their address is taken.
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi compact -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi compact -dM -E %s | FileCheck %s --check-prefix=MACRO

// MACRO: #define _M_I86CM 1
// MACRO: #define __COMPACT__ 1
// MACRO: #define __INTPTR_WIDTH__ 32
// MACRO: #define __PTRDIFF_WIDTH__ 16
// MACRO: #define __SIZE_WIDTH__ 16

_Static_assert(sizeof(int *) == 4, "far data pointer");
_Static_assert(_Alignof(int *) == 2, "2-byte aligned");
_Static_assert(sizeof(void (*)(void)) == 2, "near function pointer");
_Static_assert(sizeof(__SIZE_TYPE__) == 2, "16-bit size_t");

// CHECK: @g = global i16 0, align 2
// CHECK: @pg = global ptr addrspace(2) addrspacecast (ptr @g to ptr addrspace(2)), align 2
int g;
int *pg = &g;

// CHECK: define i16 @deref(ptr addrspace(2) noundef %p) addrspace(4)
// CHECK: load i16, ptr addrspace(2)
int deref(int *p) { return *p; }

// CHECK: define i16 @local() addrspace(4)
// CHECK: %x = alloca i16, align 2
// CHECK: %x.ascast = addrspacecast ptr %x to ptr addrspace(2)
// CHECK: call addrspace(4) i16 @deref(ptr addrspace(2) noundef %x.ascast)
int local(void) { int x = 1; return deref(&x); }

// CHECK: define i16 @diff(ptr addrspace(2) noundef %a, ptr addrspace(2) noundef %b) addrspace(4)
int diff(int *a, int *b) { return a - b; }

// CHECK: !{i32 1, !"target-abi", !"compact"}
