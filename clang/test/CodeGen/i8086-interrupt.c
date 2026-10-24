// __interrupt functions: the x86_intrcc convention, far in every model.
// A structure parameter (union INTPACK) is a pointer to the saved
// registers, without byval or dead_on_return, so stores to it stay; a call
// through a pointer to an interrupt function uses the convention too.
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi small -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi tiny -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi large -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi small -O2 -emit-llvm %s -o - | FileCheck %s --check-prefix=OPT

union INTPACK {
  struct { unsigned short gs, fs, es, ds, di, si, bp, sp, bx, dx, cx, ax, ip, cs, flags; } w;
};

void (__interrupt __far *old)();
// CHECK: @old = global ptr addrspace(5) null
void (__interrupt *old2)(void);
// CHECK: @old2 = global ptr addrspace(5) null

// CHECK: define{{.*}} x86_intrcc void @regs(ptr {{.*}}dereferenceable(30) %r) addrspace(5)
// OPT-LABEL: define{{.*}} @regs(
// OPT: store i16
void __interrupt regs(union INTPACK r) { r.w.ax = r.w.bx; }

// CHECK: define{{.*}} x86_intrcc void @msc(i16 noundef %es, i16 noundef %ds){{.*}} addrspace(5)
void __interrupt __far msc(unsigned es, unsigned ds) {}

// CHECK: define{{.*}} x86_intrcc void @chain(){{.*}} addrspace(5)
// CHECK: call x86_intrcc addrspace(5) void %{{.*}}()
void _interrupt chain(void) { old(); }

void set(void (__interrupt __far *)());
// CHECK-LABEL: define{{.*}} @use(
// CHECK: call{{.*}} void @set(ptr addrspace(5) noundef @chain)
void use(void) { set(chain); }
