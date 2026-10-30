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
