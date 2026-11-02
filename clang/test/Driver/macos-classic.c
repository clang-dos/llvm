// Driver defaults for classic Mac OS (System 7 to Mac OS 9).

// RUN: %clang -### -c --target=powerpc-apple-macos9.2 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefixes=CHECK,PPC
// RUN: %clang -### -c --target=m68k-apple-macos %s 2>&1 \
// RUN:   | FileCheck %s --check-prefixes=CHECK,M68K

// CHECK: "-cc1" "-triple"
// PPC-SAME: "powerpc-apple-macos9.2"
// M68K-SAME: "m68k-apple-macos"
// PowerPC code is always position independent (TOC); 68k code is not.
// PPC-SAME: "-mrelocation-model" "pic" "-pic-level" "2"
// M68K-SAME: "-mrelocation-model" "static"
// PPC-SAME: "-target-cpu" "601"
// CHECK-NOT: "-fno-signed-char"
// CHECK-SAME: "-internal-isystem" "{{[^"]*}}/include"
// PPC-SAME: "-internal-isystem" "{{[^"]*}}{{/|\\\\}}..{{/|\\\\}}powerpc-apple-macos{{/|\\\\}}include"
// M68K-SAME: "-internal-isystem" "{{[^"]*}}{{/|\\\\}}..{{/|\\\\}}m68k-apple-macos{{/|\\\\}}include"
// CHECK-NOT: "/usr/include"
// CHECK-SAME: "-fpascal-strings"

// RUN: %clang -### -c --target=powerpc-apple-macos9.2 --sysroot=%S/no-such \
// RUN:   %s 2>&1 | FileCheck %s --check-prefix=SYSROOT
// SYSROOT: "-internal-isystem" "{{[^"]*}}no-such{{/|\\\\}}include"

// Linking runs ld (Retro68's, found as <triple>-ld if installed) with the
// sysroot's libraries.
// RUN: %clang -### --target=powerpc-apple-macos9.2 \
// RUN:   --sysroot=%S/no-such %s -o hello 2>&1 | FileCheck %s --check-prefix=LINK
// LINK: "{{[^"]*}}ld{{(.exe)?}}" "-o" "hello" "-L{{[^"]*}}no-such{{/|\\\\}}lib" "{{[^"]*}}.o" "-lc"

// RUN: %clang -### -c --target=powerpc-apple-macos9.2 -fno-pascal-strings %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=NOPASCAL
// NOPASCAL-NOT: "-fpascal-strings"

// RUN: %clang -### -c --target=powerpc-apple-macos9.2 -funsigned-char %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=UNSIGNED
// UNSIGNED: "-fno-signed-char"

// Pascal strings stay off elsewhere.
// RUN: %clang -### -c --target=powerpc-ibm-aix %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=AIX
// AIX-NOT: "-fpascal-strings"
