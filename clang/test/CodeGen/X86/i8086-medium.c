// The medium memory model: far code (address space 5, 32-bit function
// pointers) and near data (16-bit data pointers).
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi medium -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi medium -dM -E %s | FileCheck %s --check-prefix=MACRO

// MACRO: #define _M_I86MM 1
// MACRO: #define __MEDIUM__ 1

typedef int (*fn_t)(int);
_Static_assert(sizeof(fn_t) == 4, "far function pointer");
_Static_assert(_Alignof(fn_t) == 2, "2-byte aligned");
_Static_assert(sizeof(int *) == 2, "near data pointer");

int f(int);

// CHECK: @fp = global ptr addrspace(5) @f, align 2
// CHECK: declare i16 @f(i16 noundef) addrspace(5)
fn_t fp = f;

struct S { fn_t cb; char c; };
_Static_assert(sizeof(struct S) == 6, "struct with a far function pointer");

// CHECK: define i16 @call(ptr addrspace(5) noundef %g, i16 noundef %x) addrspace(5)
// CHECK: call addrspace(5) i16 %{{.*}}(i16 noundef %{{.*}})
// CHECK: call addrspace(5) i16 @f(
int call(fn_t g, int x) { return g(x) + f(x); }

