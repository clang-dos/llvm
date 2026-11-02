// RUN: %clang_cc1 -triple powerpc-apple-macos9.2 -fpascal-strings -emit-llvm \
// RUN:   -o - %s | FileCheck %s --check-prefixes=CHECK,PPC
// RUN: %clang_cc1 -triple m68k-apple-macos -fpascal-strings -emit-llvm \
// RUN:   -o - %s | FileCheck %s --check-prefixes=CHECK,M68K

// PPC: target datalayout = "E-m:a-p:32:32-Fi32-i64:64-n32-f64:32:64"
// M68K: target datalayout = "E-m:e-p:32:16:32-i8:8:8-i16:16:16-i32:16:32-i64:16:32-f32:16:32-f64:16:32-n8:16:32-a:0:16-S16"

// long double is double.
// PPC-DAG: @ld = global double 1.000000e+00, align 8
// M68K-DAG: @ld = global double 1.000000e+00, align 2
long double ld = 1.0L;

// A Pascal string literal starts with its length byte.
// CHECK-DAG: @.str = private unnamed_addr constant [7 x i8] c"\05Hello\00"
// CHECK-DAG: @hello = global ptr @.str
const unsigned char *hello = (const unsigned char *)"\pHello";

// "pascal" is accepted on PowerPC, where it is the same as C.
#ifdef __POWERPC__
pascal short Random(void);
// PPC-LABEL: define{{.*}} i16 @call_random()
// PPC: call signext i16 @Random()
short call_random(void) { return Random(); }
#endif

// Structs passed by value and returned as on AIX for PowerPC.
struct S { int a, b, c; };
// PPC: define{{.*}} void @ret(ptr {{.*}}sret(%struct.S) align 4
// M68K: define{{.*}} void @ret(ptr {{.*}}sret(%struct.S) align 2
struct S ret(void) { struct S s = {1, 2, 3}; return s; }
