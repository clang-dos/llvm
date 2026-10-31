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

// Linking for 10.0 uses libSystem's copies of crt1.o's common symbols, and
// the compiler helpers come from libcc_dynamic.a, not libgcc_s.1.dylib.
// RUN: %clang -target powerpc-apple-macosx10.0 -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LINK100
// LINK100: "-lcrt1.o" "-commons" "use_dylibs"
// LINK100: "-lSystem" "-lcc_dynamic"
// LINK100-NOT: lgcc_s.1

// RUN: %clang -target powerpc-apple-macosx10.4 -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LINK104
// LINK104: "-lcrt1.o"
// LINK104-NOT: "-commons"
// LINK104: "-lSystem" "-lgcc_s.1"
// LINK104-NOT: lcc_dynamic
