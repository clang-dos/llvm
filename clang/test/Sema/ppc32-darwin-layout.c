// RUN: %clang_cc1 -triple powerpc-apple-macosx10.4.0 -fsyntax-only -verify \
// RUN:   -DLDOUBLE_SIZE=16 %s
// RUN: %clang_cc1 -triple powerpc-apple-macosx10.0.0 -fsyntax-only -verify \
// RUN:   -DLDOUBLE_SIZE=8 %s
// expected-no-diagnostics

// Type sizes and Apple gcc's "power" struct layout for 32-bit PowerPC Darwin.

#define CHECK(e) _Static_assert(e, #e)

CHECK(sizeof(_Bool) == 4 && _Alignof(_Bool) == 4);
CHECK(sizeof(long) == 4 && sizeof(void *) == 4);
CHECK(sizeof(long long) == 8 && _Alignof(long long) == 8);
CHECK(sizeof(double) == 8 && _Alignof(double) == 8);
CHECK(sizeof(long double) == LDOUBLE_SIZE);
CHECK(sizeof(__SIZE_TYPE__) == 4);

// A double that is not the first member is only word aligned.
struct int_double { int i; double d; };
CHECK(__builtin_offsetof(struct int_double, d) == 4);
CHECK(sizeof(struct int_double) == 12 && _Alignof(struct int_double) == 4);

// A record that starts with a double is 8-byte aligned.
struct double_int { double d; int i; };
CHECK(sizeof(struct double_int) == 16 && _Alignof(struct double_int) == 8);

// The rule looks through arrays.
struct int_darray { int i; double d[2]; };
CHECK(__builtin_offsetof(struct int_darray, d) == 4);
struct darray_int { double d[2]; int i; };
CHECK(sizeof(struct darray_int) == 24 && _Alignof(struct darray_int) == 8);

// A nested record keeps its own alignment.
struct nested { int i; struct double_int r; };
CHECK(__builtin_offsetof(struct nested, r) == 8);

// long long keeps its natural alignment.
struct char_ll { char c; long long ll; };
CHECK(__builtin_offsetof(struct char_ll, ll) == 8);
