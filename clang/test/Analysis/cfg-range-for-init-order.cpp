// RUN: %clang_analyze_cc1 -std=c++17 -analyzer-checker=debug.DumpCFG -analyzer-config cfg-rich-constructors=false %s > %t 2>&1
// RUN: FileCheck --input-file=%t %s
// RUN: %clang_analyze_cc1 -std=c++17 -analyzer-checker=debug.DumpCFG -analyzer-config cfg-rich-constructors=true %s > %t 2>&1
// RUN: FileCheck --input-file=%t %s

struct Begin {
  ~Begin();
};

struct End {
  ~End();
};

bool operator!=(const Begin &, const End &);
void operator++(Begin &);
int &operator*(const Begin &);

struct Range {
  Begin begin();
  End end();
};

// The cleanup block appears before the initialization block in the dump.
// Iterators must initialize begin before end and be destroyed in reverse order.
//
// CHECK-LABEL: void range_for_init_order(
// CHECK: .~End() (Implicit destructor)
// CHECK: .~Begin() (Implicit destructor)
// CHECK: auto &&__range{{[0-9]*}} = range;
// CHECK: auto __begin{{[0-9]*}} = {{.*}}.begin();
// CHECK: auto __end{{[0-9]*}} = {{.*}}.end();
void range_for_init_order(Range range) {
  for (int &value : range)
    (void)value;
}