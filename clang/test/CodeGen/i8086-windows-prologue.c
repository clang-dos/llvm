// -mwindows-prologue= marks far functions for the 16-bit Windows entry
// sequence: exported ones (exported) or all (all). __loadds (and _loadds)
// marks a function that loads DGROUP into DS.
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi large -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,NONE
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi large -mwindows-prologue=exported -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,EXPORTED
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi large -mwindows-prologue=all -emit-llvm %s -o - | FileCheck %s --check-prefix=ALL

// CHECK: define dllexport i16 @exported() addrspace(5) #[[EXP:[0-9]+]]
int __export exported(void) { return 0; }
// CHECK: define{{.*}} i16 @plain() addrspace(5) #[[PLAIN:[0-9]+]]
int plain(void) { return 0; }
// CHECK: define{{.*}} i16 @ld() addrspace(5) #[[LD:[0-9]+]]
int __loadds ld(void) { return 0; }
// CHECK: define{{.*}} i16 @ld2() addrspace(5) #[[LD]]
int _loadds ld2(void) { return 0; }

// NONE-NOT: "i8086-windows-prologue"
// EXPORTED: attributes #[[EXP]] = {{{.*}}"i8086-windows-prologue"
// EXPORTED: attributes #[[PLAIN]] = {
// EXPORTED-NOT: "i8086-windows-prologue"
// EXPORTED-SAME: }
// ALL: define dllexport i16 @exported() addrspace(5) #[[A:[0-9]+]]
// ALL: define{{.*}} i16 @plain() addrspace(5) #[[A]]
// ALL: attributes #[[A]] = {{{.*}}"i8086-windows-prologue"
// CHECK: attributes #[[LD]] = {{{.*}}"i8086-loadds"
