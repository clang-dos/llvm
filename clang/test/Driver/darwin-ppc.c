// Check that the driver accepts 32-bit PowerPC Mac OS X targets.

// RUN: %clang -target powerpc-apple-darwin8 -c %s -### 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TRIPLE
// TRIPLE: "-cc1" "-triple" "powerpc-apple-macosx10.4.0"
// TRIPLE-NOT: -funwind-tables

// RUN: %clang -target powerpc-apple-darwin8 -arch ppc750 -c %s -### 2>&1 \
// RUN:   | FileCheck %s --check-prefix=G3
// G3: "-cc1" "-triple" "powerpc-apple-macosx10.4.0"
// G3-SAME: "-target-cpu" "750"

// RUN: %clang -target powerpc-apple-darwin8 -arch ppc7400 -c %s -### 2>&1 \
// RUN:   | FileCheck %s --check-prefix=G4
// G4: "-target-cpu" "7400"

// RUN: %clang -target powerpc-apple-macosx10.0 -c %s -### 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TENZERO
// TENZERO: "-cc1" "-triple" "powerpc-apple-macosx10.0.0"

// Linking for 10.0 uses libSystem's copies of crt1.o's common symbols.
// Without our builtins installed, the compiler helpers come from
// libcc_dynamic.a, not libgcc_s.1.dylib.
// RUN: %clang -target powerpc-apple-macosx10.0 -resource-dir=%t.empty -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LINK100
// LINK100: "-lcrt1.o" "-commons" "use_dylibs"
// LINK100: "-lSystem" "-lcc_dynamic"
// LINK100-NOT: lgcc_s.1

// With libclang_rt.ppc_osx.a in the resource directory, 10.0 to 10.3 link
// it alone, and 10.4 and later link it after libgcc_s.1.dylib.
// RUN: rm -rf %t && mkdir -p %t/res/lib/darwin
// RUN: touch %t/res/lib/darwin/libclang_rt.ppc_osx.a
// RUN: %clang -target powerpc-apple-macosx10.0 -resource-dir=%t/res -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=RT100
// RUN: %clang -target powerpc-apple-macosx10.2 -resource-dir=%t/res -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=RT100
// RUN: %clang -target powerpc-apple-darwin7 -resource-dir=%t/res -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=RT100
// RT100: "-lSystem" "{{.*}}libclang_rt.ppc_osx.a"
// RT100-NOT: lcc_dynamic
// RT100-NOT: lgcc_s.1
// RUN: %clang -target powerpc-apple-macosx10.4 -resource-dir=%t/res -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=RT104
// RT104: "-lSystem" "-lgcc_s.1" "{{.*}}libclang_rt.ppc_osx.a"

// Long double is 64-bit before 10.4 and 128-bit from 10.4.
// RUN: %clang -target powerpc-apple-macosx10.3 -E -dM %s -o - \
// RUN:   | FileCheck %s --check-prefix=LD103
// LD103: #define __LDBL_MANT_DIG__ 53
// RUN: %clang -target powerpc-apple-darwin8 -E -dM %s -o - \
// RUN:   | FileCheck %s --check-prefix=LD104
// LD104: #define __LDBL_MANT_DIG__ 106

// Apple's ld64 from cctools-port is used when it is found, here through -B.
// UNSUPPORTED: system-windows
// RUN: mkdir -p %t/bin && touch %t/bin/powerpc-apple-darwin8-ld
// RUN: chmod +x %t/bin/powerpc-apple-darwin8-ld
// RUN: %clang -target powerpc-apple-macosx10.4 -B %t/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LD64
// LD64: "{{.*}}bin{{/|\\\\}}powerpc-apple-darwin8-ld"

// RUN: %clang -target powerpc-apple-macosx10.4 -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LINK104
// LINK104: "-lcrt1.o"
// LINK104-NOT: "-commons"
// LINK104: "-lSystem" "-lgcc_s.1"
// LINK104-NOT: lcc_dynamic

// Objective-C uses the fragile runtime, and before 10.2, whose CoreFoundation
// first has __CFConstantStringClassReference, NSConstantString literals.
// RUN: %clang -target powerpc-apple-macosx10.0 -x objective-c -c %s -### 2>&1 \
// RUN:   | FileCheck %s --check-prefix=OBJC100
// OBJC100: "-fno-constant-cfstrings"
// OBJC100: "-fobjc-runtime=macosx-fragile-10.0.0"
// RUN: %clang -target powerpc-apple-macosx10.2 -x objective-c -c %s -### 2>&1 \
// RUN:   | FileCheck %s --check-prefix=OBJC102
// OBJC102-NOT: "-fno-constant-cfstrings"
// OBJC102: "-fobjc-runtime=macosx-fragile-10.2.0"
// OBJC102-NOT: "-fno-constant-cfstrings"
// RUN: %clang -target powerpc-apple-macosx10.0 -fconstant-cfstrings -c %s -### 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CFSTR
// CFSTR-NOT: "-fno-constant-cfstrings"
