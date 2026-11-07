// On 32-bit x86, GCC (and so MinGW and Cygwin) passes a C++ empty class
// argument in a 4-byte stack slot instead of dropping it. Callee-cleanup
// functions must pop that slot too, or calls across a GCC/Clang boundary,
// such as into libstdc++'s tag-dispatched members, corrupt the stack.

// REQUIRES: x86-registered-target

// RUN: %clang_cc1 -emit-llvm -triple i686-w64-mingw32 %s -o - | FileCheck --check-prefix=GCC %s
// RUN: %clang_cc1 -emit-llvm -triple i686-pc-cygwin %s -o - | FileCheck --check-prefix=GCC %s
// RUN: %clang_cc1 -emit-llvm -triple i686-w64-mingw32 -fclang-abi-compat=23 %s -o - | FileCheck --check-prefix=OLD %s
// RUN: %clang_cc1 -emit-llvm -triple i686-pc-windows-msvc %s -o - | FileCheck --check-prefix=MSVC %s
// RUN: %clang_cc1 -S -triple i686-w64-mingw32 -O2 %s -o - | FileCheck --check-prefix=ASM %s

struct E {};
struct EB : E {};
struct EF { E e; };
struct EA { int a[0]; };

void use(int);

// GCC: define {{.*}}void @_Z2f1i1Ei(i32 noundef %a, ptr noundef byval(%struct.E) align 4 %e, i32 noundef %b)
// OLD: define {{.*}}void @_Z2f1i1Ei(i32 noundef %a, i32 noundef %b)
// MSVC: define {{.*}}void @"?f1@@YAXHUE@@H@Z"(i32 noundef %a, ptr noundef byval(%struct.E) align 4 %e, i32 noundef %b)
void f1(int a, E e, int b) { use(a); use(b); }

// Bases and members that are themselves empty.
// GCC: define {{.*}}void @_Z2f22EB2EF(ptr noundef byval(%struct.EB) align 4 %b, ptr noundef byval(%struct.EF) align 4 %f)
// OLD: define {{.*}}void @_Z2f22EB2EF(ptr noundef byval(%struct.EF) align 4 %f)
void f2(EB b, EF f) {}

// A record with no storage at all is still dropped, as in GCC.
// GCC: define {{.*}}void @_Z2f32EAi(i32 noundef %n)
void f3(EA a, int n) { use(n); }

// The libstdc++ case: std::basic_string::_M_construct(const char *,
// const char *, std::forward_iterator_tag) is a thiscall member.
struct S { void m(const char *, const char *, E); };
// GCC: define {{.*}}void @_ZN1S1mEPKcS1_1E(ptr {{.*}} %this, ptr noundef %a, ptr noundef %b, ptr noundef byval(%struct.E) align 4 %0)
// OLD: define {{.*}}x86_thiscallcc void @_ZN1S1mEPKcS1_1E(ptr {{.*}} %this, ptr noundef %a, ptr noundef %b)
// ASM-LABEL: __ZN1S1mEPKcS1_1E:
// ASM: retl $12
void S::m(const char *a, const char *b, E) { use((int)a); }

// The stdcall decoration already counted the slot; the cleanup must agree.
// GCC: define {{.*}}x86_stdcallcc void @"\01__Z2f4i1E@8"(i32 noundef %a, ptr noundef byval(%struct.E) align 4 %e)
// ASM-LABEL: __Z2f4i1E@8:
// ASM: retl $8
void __attribute__((stdcall)) f4(int a, E e) { use(a); }

// Callers pass the slot too.
// GCC-LABEL: define {{.*}}void @_Z4callv()
// GCC: call void @_Z2f1i1Ei(i32 noundef 1, ptr noundef byval(%struct.E) align 4 %{{.*}}, i32 noundef 2)
void call() { f1(1, E(), 2); }
