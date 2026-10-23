// The i8086 triple defaults to the 8086 CPU and accepts the 16-bit CPUs.
// RUN: %clang --target=i8086-unknown-unknown -### -c %s 2>&1 | FileCheck %s --check-prefix=DEFAULT
// RUN: %clang --target=i8086-unknown-unknown -march=i286 -### -c %s 2>&1 | FileCheck %s --check-prefix=I286
// RUN: not %clang --target=i386-unknown-unknown -march=i8086 -c %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=I386

// DEFAULT: "-triple" "i8086-unknown-unknown"
// DEFAULT-SAME: "-target-cpu" "i8086"
// I286: "-target-cpu" "i286"
// I386: error: unknown target CPU 'i8086'
