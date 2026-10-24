// The pointer to a returned structure is far in the far-data models, and in
// every model on 16-bit Windows, where a DLL's stack is not in DGROUP.
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi small -emit-llvm %s -o - \
// RUN:   | FileCheck %s --check-prefix=NEAR
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi large -emit-llvm %s -o - \
// RUN:   | FileCheck %s --check-prefix=FAR
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi compact -emit-llvm %s -o - \
// RUN:   | FileCheck %s --check-prefix=FAR
// RUN: %clang_cc1 -triple i8086-pc-win16 -target-abi small -emit-llvm %s -o - \
// RUN:   | FileCheck %s --check-prefixes=FAR,WINSMALL
// RUN: %clang_cc1 -triple i8086-pc-win16 -target-abi medium -emit-llvm %s -o - \
// RUN:   | FileCheck %s --check-prefixes=FAR,WINSMALL

struct big { int a[8]; };

// NEAR: define{{.*}} void @make(ptr dead_on_unwind noalias writable sret(%struct.big) align 2 %agg.result, i16 noundef %n)
// FAR: define{{.*}} void @make(ptr addrspace(2) dead_on_unwind noalias writable sret(%struct.big) align 2 %agg.result, i16 noundef %n)
struct big make(int n) {
  struct big b = {{n}};
  return b;
}

// NEAR-LABEL: define{{.*}} @use(
// NEAR: call{{.*}} void @make(ptr dead_on_unwind writable sret(%struct.big) align 2 %{{.*}}, i16
// FAR-LABEL: define{{.*}} @use(
// FAR: %[[P:.*]] = addrspacecast ptr %tmp to ptr addrspace(2)
// FAR: call{{.*}} void @make(ptr addrspace(2) dead_on_unwind writable sret(%struct.big) align 2 %[[P]], i16
int use(void) { return make(3).a[0]; }

// On win16 in the near-data models the named return value is a local of
// its own, as its address is near, and is copied to the far return slot.
// NEAR-LABEL: define{{.*}} void @named(
// NEAR-NOT:   alloca %struct.big
// NEAR:       ret void
// FAR-LABEL:  define{{.*}} void @named(
// WINSMALL:     %b = alloca %struct.big
// WINSMALL:     call{{.*}} @llvm.memcpy.p2.p0.i16(ptr addrspace(2) align 2 %agg.result, ptr align 2 %b, i16 16, i1 false)
// FAR:          ret void
struct big named(int n) {
  struct big b;
  for (int i = 0; i < 8; i++)
    b.a[i] = n;
  return b;
}
