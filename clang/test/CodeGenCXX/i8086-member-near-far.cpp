// i8086 member functions with __far before the name are far calls, and with
// __far after the parameter list take a far `this`.
// RUN: %clang_cc1 -triple i8086-pc-win16 -target-abi small -emit-llvm -o - %s | FileCheck %s

struct S {
  int x;
  S() __far;
  int get() __far;
  int __far call();
};

S::S() __far : x(1) {}
// CHECK: define {{.*}}void @_ZNU3AS21SC2Ev(ptr addrspace(2) {{.*}}%this) {{.*}}addrspace(4)
int S::get() __far { return x; }
// CHECK: define {{.*}}i16 @_ZNU3AS21S3getEv(ptr addrspace(2) {{.*}}%this) {{.*}}addrspace(4)
int S::call() { return x; }
// CHECK: define {{.*}}i16 @_ZN1S4callEv(ptr {{.*}}%this) {{.*}}addrspace(5)

// A local object gets a far `this` from its address, which the backend makes
// from SS.
int use() {
  S s;
  return s.get() + s.call();
}
// CHECK-LABEL: define {{.*}}@_Z3usev
// CHECK: %s = alloca %struct.S
// CHECK: [[FAR:%.*]] = addrspacecast ptr %s to ptr addrspace(2)
// CHECK: call addrspace(4) void @_ZNU3AS21SC1Ev(ptr addrspace(2) {{.*}}[[FAR]])
// CHECK: [[FAR1:%.*]] = addrspacecast ptr %s to ptr addrspace(2)
// CHECK: call {{.*}}addrspace(4) i16 @_ZNU3AS21S3getEv(ptr addrspace(2) {{.*}}[[FAR1]])
// CHECK: call {{.*}}addrspace(5) i16 @_ZN1S4callEv(ptr {{.*}}%s)
