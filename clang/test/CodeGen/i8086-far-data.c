// In the far-data memory models, a variable larger than the data threshold
// is far data in a segment of its own: address space 2, or 3 in the huge
// models. Smaller variables, and every variable in the near-data models,
// stay in DGROUP.
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi compact -mlarge-data-threshold=32767 -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,FAR
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi large -mlarge-data-threshold=32767 -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,FAR
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge -mlarge-data-threshold=32767 -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,HUGE
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge16 -mlarge-data-threshold=32767 -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,HUGE
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi small -mlarge-data-threshold=32767 -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,NEAR
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi compact -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,NEAR
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi compact -mlarge-data-threshold=100 -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,T100

// FAR-DAG: @big = addrspace(2) global [20000 x i16] zeroinitializer
// HUGE-DAG: @big = addrspace(3) global [20000 x i16] zeroinitializer
// NEAR-DAG: @big = global [20000 x i16] zeroinitializer
int big[20000];
// FAR-DAG: @limit = global [32767 x i8] zeroinitializer
// HUGE-DAG: @limit = global [32767 x i8] zeroinitializer
// T100-DAG: @limit = addrspace(2) global [32767 x i8] zeroinitializer
char limit[32767];
// FAR-DAG: @table = addrspace(2) constant <{ i8, [39999 x i8] }>
// HUGE-DAG: @table = addrspace(3) constant <{ i8, [39999 x i8] }>
const char table[40000] = {1};
// CHECK-DAG: @small = global [100 x i8] zeroinitializer
char small[100];
// An extern variable of unknown size may be far data elsewhere. A far
// reference reaches near data too, so it is far.
// FAR-DAG: @ext = external addrspace(2) global [0 x i8]
// HUGE-DAG: @ext = external addrspace(3) global [0 x i8]
// NEAR-DAG: @ext = external global [0 x i8]
extern char ext[];
// Once its size is known it follows the threshold; earlier uses are cast.
// FAR-DAG: @later = global [10 x i8] zeroinitializer
// FAR-DAG: load i8, ptr addrspace(2) getelementptr inbounds nuw (i8, ptr addrspace(2) addrspacecast (ptr @later to ptr addrspace(2)), i16 3)
extern char later[];
char use_later(void) { return later[3]; }
char later[10];
// A pointer into far data is a far pointer.
// FAR-DAG: @p = global ptr addrspace(2) getelementptr (i8, ptr addrspace(2) @big, i64 10)
// HUGE-DAG: @p = global ptr addrspace(3) getelementptr (i8, ptr addrspace(3) @big, i64 10)
int *p = &big[5];

// FAR-LABEL: define{{.*}} @get(
// FAR: load i16, ptr addrspace(2)
int get(int i) { return big[i] + ext[i]; }
