// C++ in the i8086 memory models with far code or far data. A vtable
// component is as wide as the larger of a code and a data pointer, the vptr
// and this are default data pointers, a member function pointer holds a code
// pointer, and the runtime library takes default data and code pointers.
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi small -emit-llvm %s -o - | FileCheck %s --check-prefixes=SMALL
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi medium -emit-llvm %s -o - | FileCheck %s --check-prefixes=MEDIUM
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi compact -emit-llvm %s -o - | FileCheck %s --check-prefixes=COMPACT
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge -emit-llvm %s -o - | FileCheck %s --check-prefixes=HUGE

namespace std { class type_info; }

struct A { virtual ~A(); virtual int f(); int a; };
struct B { virtual int g(); };
struct C : A, B { int f() override; int g() override; };
int C::f() { return 3; }

// SMALL: @mfp = global { i16, i16 } { i16 5, i16 0 }
// MEDIUM: @mfp = global { i32, i16 } { i32 9, i16 0 }
// COMPACT: @mfp = global { i16, i16 } { i16 9, i16 0 }
// HUGE: @mfp = global { i32, i32 } { i32 9, i32 0 }
int (A::*mfp)() = &A::f;

// The type_info vptr and pointers are default data pointers. In the far-data
// models, vtables and type_info objects are far (huge in huge), outside
// DGROUP.
// MEDIUM: @_ZTI1C = constant { ptr, ptr, i16, i16, ptr, i32, ptr, i32 } { ptr getelementptr inbounds (i8, ptr @_ZTVN10__cxxabiv121__vmi_class_type_infoE, i16 8), ptr @_ZTS1C,
// COMPACT: @_ZTI1C = addrspace(2) constant { ptr addrspace(2), ptr addrspace(2), i16, i16, ptr addrspace(2), i32, ptr addrspace(2), i32 } { ptr addrspace(2) getelementptr inbounds (i8, ptr addrspace(2) @_ZTVN10__cxxabiv121__vmi_class_type_infoE, i16 8), ptr addrspace(2) @_ZTS1C,
// HUGE: @_ZTI1C = addrspace(3) constant { ptr addrspace(3), ptr addrspace(3), i16, i16, ptr addrspace(3), i32, ptr addrspace(3), i32 } { ptr addrspace(3) getelementptr inbounds (i8, ptr addrspace(3) @_ZTVN10__cxxabiv121__vmi_class_type_infoE, i32 8), ptr addrspace(3) @_ZTS1C,

// SMALL: @_ZTV1C = constant { [6 x ptr], [3 x ptr] } { [6 x ptr] [ptr null, ptr @_ZTI1C, {{.*}}], [3 x ptr] [ptr inttoptr (i16 -4 to ptr), ptr @_ZTI1C, {{.*}}] }
// MEDIUM: @_ZTV1C = constant { [6 x ptr addrspace(5)], [3 x ptr addrspace(5)] } { [6 x ptr addrspace(5)] [ptr addrspace(5) null, ptr addrspace(5) addrspacecast (ptr @_ZTI1C to ptr addrspace(5)), ptr addrspace(5) @_ZN1CD1Ev, {{.*}}], [3 x ptr addrspace(5)] [ptr addrspace(5) inttoptr (i32 -4 to ptr addrspace(5)), ptr addrspace(5) addrspacecast (ptr @_ZTI1C to ptr addrspace(5)), ptr addrspace(5) @_ZThn4_N1C1gEv] }
// COMPACT: @_ZTV1C = addrspace(2) constant { [6 x ptr addrspace(2)], [3 x ptr addrspace(2)] } { [6 x ptr addrspace(2)] [ptr addrspace(2) null, ptr addrspace(2) @_ZTI1C, ptr addrspace(2) addrspacecast (ptr addrspace(4) @_ZN1CD1Ev to ptr addrspace(2)), {{.*}}], [3 x ptr addrspace(2)] [ptr addrspace(2) inttoptr (i32 -6 to ptr addrspace(2)), {{.*}}] }
// HUGE: @_ZTV1C = addrspace(3) constant { [6 x ptr addrspace(5)], [3 x ptr addrspace(5)] } { [6 x ptr addrspace(5)] [ptr addrspace(5) null, ptr addrspace(5) addrspacecast (ptr addrspace(3) @_ZTI1C to ptr addrspace(5)), ptr addrspace(5) @_ZN1CD1Ev, {{.*}}], [3 x ptr addrspace(5)] [ptr addrspace(5) inttoptr (i32 -6 to ptr addrspace(5)), ptr addrspace(5) addrspacecast (ptr addrspace(3) @_ZTI1C to ptr addrspace(5)), ptr addrspace(5) @_ZThn6_N1C1gEv] }

// The destructor registered with __cxa_atexit is a code pointer.
// MEDIUM: call addrspace(5) i16 @__cxa_atexit(ptr addrspace(5) @_ZN1SD1Ev, ptr @s, ptr @__dso_handle)
// COMPACT: call addrspace(4) i16 @__cxa_atexit(ptr addrspace(4) @_ZN1SD1Ev, ptr addrspace(2) addrspacecast (ptr @s to ptr addrspace(2)), ptr addrspace(2) addrspacecast (ptr @__dso_handle to ptr addrspace(2)))
// HUGE: call addrspace(5) i16 @__cxa_atexit(ptr addrspace(5) @_ZN1SD1Ev, ptr addrspace(3) addrspacecast (ptr @s to ptr addrspace(3)), ptr addrspace(3) addrspacecast (ptr @__dso_handle to ptr addrspace(3)))
struct S { ~S(); } s;

// MEDIUM-LABEL: define{{.*}} i16 @_Z5callfP1A(
// MEDIUM: %vtable = load ptr, ptr %{{.*}}, align 2
// MEDIUM: %vfn = getelementptr inbounds ptr addrspace(5), ptr %vtable, i64 2
// MEDIUM: load ptr addrspace(5), ptr %vfn
// COMPACT-LABEL: define{{.*}} i16 @_Z5callfP1A(
// COMPACT: %vtable = load ptr addrspace(2), ptr addrspace(2) %{{.*}}, align 2
// COMPACT: %vfn = getelementptr inbounds ptr addrspace(2), ptr addrspace(2) %vtable, i64 2
// COMPACT: load ptr addrspace(4), ptr addrspace(2) %vfn
// HUGE-LABEL: define{{.*}} i16 @_Z5callfP1A(
// HUGE: %vtable = load ptr addrspace(3), ptr addrspace(3) %{{.*}}, align 2
// HUGE: %vfn = getelementptr inbounds ptr addrspace(5), ptr addrspace(3) %vtable, i64 2
// HUGE: load ptr addrspace(5), ptr addrspace(3) %vfn
int callf(A *p) { return p->f(); }

// MEDIUM-LABEL: define{{.*}} @_Z4downP1B(
// MEDIUM: call addrspace(5) ptr @__dynamic_cast(ptr %{{.*}}, ptr @_ZTI1B, ptr @_ZTI1C, i16 4)
// COMPACT-LABEL: define{{.*}} @_Z4downP1B(
// COMPACT: call addrspace(4) ptr addrspace(2) @__dynamic_cast(ptr addrspace(2) %{{.*}}, ptr addrspace(2) @_ZTI1B, ptr addrspace(2) @_ZTI1C, i16 6)
// HUGE-LABEL: define{{.*}} @_Z4downP1B(
// HUGE: call addrspace(5) ptr addrspace(3) @__dynamic_cast(ptr addrspace(3) %{{.*}}, ptr addrspace(3) @_ZTI1B, ptr addrspace(3) @_ZTI1C, i32 6)
C *down(B *p) { return dynamic_cast<C *>(p); }

// The type_info is the component before the address point.
// MEDIUM-LABEL: define{{.*}} @_Z2tiP1A(
// MEDIUM: getelementptr inbounds ptr addrspace(5), ptr %vtable, i64 -1
// COMPACT-LABEL: define{{.*}} @_Z2tiP1A(
// COMPACT: getelementptr inbounds ptr addrspace(2), ptr addrspace(2) %vtable, i64 -1
// COMPACT: load ptr addrspace(2), ptr
// HUGE-LABEL: define{{.*}} @_Z2tiP1A(
// HUGE: getelementptr inbounds ptr addrspace(5), ptr addrspace(3) %vtable, i64 -1
// HUGE: load ptr addrspace(3), ptr
const std::type_info *ti(A *p) { return &typeid(*p); }

// A virtual member function pointer holds the vtable offset plus one.
// MEDIUM-LABEL: define{{.*}} @_Z5callmP1AMS_FivE(
// MEDIUM: %memptr.virtualfn = load ptr addrspace(5), ptr
// MEDIUM: %memptr.nonvirtualfn = inttoptr i32 %{{.*}} to ptr addrspace(5)
// HUGE-LABEL: define{{.*}} @_Z5callmP1AMS_FivE(
// HUGE: %memptr.virtualfn = load ptr addrspace(5), ptr
// HUGE: %memptr.nonvirtualfn = inttoptr i32 %{{.*}} to ptr addrspace(5)
int callm(A *p, int (A::*m)()) { return (p->*m)(); }
