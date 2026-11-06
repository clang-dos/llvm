// RUN: %clang_cc1 -triple powerpc-apple-macosx10.4 -fobjc-runtime=macosx-fragile-10.4 -Wno-objc-root-class -emit-llvm -o - %s | FileCheck %s

// 32-bit PowerPC Mac OS X lays out instance variables with gcc's "power"
// alignment, like struct members: a double that is not the first member is
// only word aligned. An instance variable is never first, because isa or the
// superclass's instance variables come before it.

@interface Root {
  Class isa;
  double d;
}
@end
@implementation Root
@end

@interface Child : Root {
  char c;
  double e;
}
@end
@implementation Child
@end

// Root: isa at 0, d at 4, size 12.
// CHECK: @OBJC_INSTANCE_VARIABLES_Root = {{.*}} i32 0 }, {{.*}} i32 4 }
// CHECK: @OBJC_CLASS_Root = {{.*}} i32 12,
// Child: c at 12, e at 16, size 24.
// CHECK: @OBJC_INSTANCE_VARIABLES_Child = {{.*}} i32 12 }, {{.*}} i32 16 }
// CHECK: @OBJC_CLASS_Child = {{.*}} i32 24,
