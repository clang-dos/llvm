// C++ in the i8086 huge model with a 16-bit size_t (target ABI huge16), where
// ptrdiff_t is wider than size_t.
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge16 -emit-llvm -o - %s | FileCheck %s

// A range-based for over an array: the bound is a ptrdiff_t.
// CHECK-LABEL: define{{.*}} i16 @_Z5countv(
// CHECK: getelementptr inbounds i16, ptr addrspace(3) %{{.*}}, i32 10
int count() {
  int a[10] = {}, n = 0;
  for (int x : a)
    n += x + 1;
  return n;
}

// The array bound deduces as a size_t.
template <class T, __SIZE_TYPE__ N> constexpr __SIZE_TYPE__ size(T (&)[N]) { return N; }
int b[40000 / sizeof(int)];
static_assert(size(b) == 20000, "");

// CHECK-LABEL: define{{.*}} @_Z4diffPiS_(
// CHECK: sdiv exact i32 %{{.*}}, 2
long diff(int *p, int *q) { return p - q; }

// new[] takes a 16-bit size_t.
// CHECK-LABEL: define{{.*}} @_Z4makej(
// CHECK: call{{.*}} @_Znaj(i16 noundef
int *make(unsigned n) { return new int[n]; }
