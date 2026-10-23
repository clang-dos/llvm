// The huge memory model: far code and huge data pointers (address space 3),
// which address objects over 64 KiB, so size_t and ptrdiff_t are 32 bits.
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge -dM -E %s | FileCheck %s --check-prefix=MACRO

// MACRO: #define _M_I86HM 1
// MACRO: #define __HUGE__ 1
// MACRO: #define __INTPTR_WIDTH__ 32
// MACRO: #define __PTRDIFF_WIDTH__ 32
// MACRO: #define __SIZE_WIDTH__ 32

_Static_assert(sizeof(int *) == 4, "huge data pointer");
_Static_assert(sizeof(void (*)(void)) == 4, "far function pointer");
_Static_assert(sizeof(__SIZE_TYPE__) == 4, "32-bit size_t");
_Static_assert(sizeof(char[100000]) == 100000, "objects over 64 KiB");

// CHECK: @g = global i16 0, align 2
int g;

// CHECK: define signext i8 @at(ptr addrspace(3) noundef %p, i32 noundef %i) addrspace(5)
// CHECK: getelementptr inbounds i8, ptr addrspace(3) %{{.*}}, i32 %{{.*}}
char at(char *p, long i) { return p[i]; }

// CHECK: define i32 @diff(ptr addrspace(3) noundef %a, ptr addrspace(3) noundef %b) addrspace(5)
__PTRDIFF_TYPE__ diff(char *a, char *b) { return a - b; }

// CHECK: define ptr addrspace(3) @addr() addrspace(5)
// CHECK: ret ptr addrspace(3) addrspacecast (ptr @g to ptr addrspace(3))
int *addr(void) { return &g; }

// CHECK: !{i32 1, !"target-abi", !"huge"}
