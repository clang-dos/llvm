// The huge memory model with a 16-bit size_t, as in Open Watcom (-msize-t=16,
// target ABI huge16): huge data pointers, but objects stay under 64 KiB, and
// ptrdiff_t is 32 bits.
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge16 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge16 -dM -E %s | FileCheck %s --check-prefix=MACRO

// MACRO: #define _M_I86HM 1
// MACRO: #define __HUGE__ 1
// MACRO: #define __INTPTR_WIDTH__ 32
// MACRO: #define __PTRDIFF_WIDTH__ 32
// MACRO: #define __SIZE_WIDTH__ 16

_Static_assert(sizeof(int *) == 4, "huge data pointer");
_Static_assert(sizeof(__SIZE_TYPE__) == 2, "16-bit size_t");
_Static_assert(sizeof(__PTRDIFF_TYPE__) == 4, "32-bit ptrdiff_t");

// An unsigned index is zero-extended to the 32-bit index of a huge pointer.
// CHECK-LABEL: define signext i8 @at(ptr addrspace(3) noundef %p, i16 noundef %i) addrspace(5)
// CHECK: %idxprom = zext i16 %{{.*}} to i32
// CHECK: getelementptr inbounds nuw i8, ptr addrspace(3) %{{.*}}, i32 %idxprom
char at(char *p, __SIZE_TYPE__ i) { return p[i]; }

// CHECK-LABEL: define i32 @diff(ptr addrspace(3) noundef %a, ptr addrspace(3) noundef %b) addrspace(5)
// CHECK: %sub.ptr.sub = sub i32
// CHECK: sdiv exact i32 %sub.ptr.sub, 2
__PTRDIFF_TYPE__ diff(short *a, short *b) { return a - b; }

// CHECK-LABEL: define void @copy(
// CHECK: call addrspace(5) void @llvm.memcpy.p3.p3.i16(ptr addrspace(3) align 1 %{{.*}}, ptr addrspace(3) align 1 %{{.*}}, i16 %{{.*}}, i1 false)
void copy(char *d, const char *s, __SIZE_TYPE__ n) { __builtin_memcpy(d, s, n); }

// CHECK: !{i32 1, !"target-abi", !"huge16"}
