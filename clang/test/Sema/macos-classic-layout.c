// RUN: %clang_cc1 -triple powerpc-apple-macos9.2 -fsyntax-only -verify \
// RUN:   -DPPC %s
// RUN: %clang_cc1 -triple m68k-apple-macos7.5 -fsyntax-only -verify \
// RUN:   -DM68K %s
// expected-no-diagnostics

// Type sizes and struct layout for classic Mac OS, as in MPW (MrC and SC),
// CodeWarrior and Retro68. PowerPC uses the AIX "power" rule; 68k aligns
// everything of 2 bytes or more to 2 ("mac68k" alignment).

#define CHECK(e) _Static_assert(e, #e)

CHECK(sizeof(int) == 4 && sizeof(long) == 4 && sizeof(void *) == 4);
CHECK(sizeof(short) == 2 && sizeof(long long) == 8);
CHECK(sizeof(double) == 8 && sizeof(long double) == 8);
CHECK(sizeof(_Bool) == 1);
CHECK((char)-1 < 0); // char is signed on both CPUs.
CHECK(sizeof(__SIZE_TYPE__) == 4);

struct c_d_i { char c; double d; int i; };
struct c_i { char c; int i; };
struct c_s { char c; short s; };
struct c_ll { char c; long long ll; };
struct d_c { double d; char c; };
struct c_f { char c; float f; };
struct c_p { char c; void *p; };
struct c { char c; };
struct c_c_c { char a, b, c; };

#ifdef PPC
// As on AIX, double has a 4-byte ABI alignment (_Alignof) and an 8-byte
// preferred alignment (__alignof__).
CHECK(_Alignof(int) == 4 && _Alignof(double) == 4 && __alignof__(double) == 8);
// A double that is not the first member is only word aligned.
CHECK(__builtin_offsetof(struct c_d_i, d) == 4);
CHECK(sizeof(struct c_d_i) == 16 && _Alignof(struct c_d_i) == 4);
// A struct that starts with a double is 8-byte aligned.
CHECK(sizeof(struct d_c) == 16 && __alignof__(struct d_c) == 8);
CHECK(sizeof(struct c_i) == 8 && _Alignof(struct c_i) == 4);
CHECK(__builtin_offsetof(struct c_ll, ll) == 8);
CHECK(sizeof(struct c_ll) == 16);
CHECK(sizeof(struct c) == 1 && sizeof(struct c_c_c) == 3);

// Apple's headers switch to 68k layout for structures shared with 68k code.
#pragma options align=mac68k
struct m68k_c_d_i { char c; double d; int i; };
struct m68k_c { char c; };
#pragma options align=reset
CHECK(sizeof(struct m68k_c_d_i) == 14 && _Alignof(struct m68k_c_d_i) == 2);
CHECK(__builtin_offsetof(struct m68k_c_d_i, d) == 2);

// align=power is the default; align=natural aligns double to 8.
#pragma options align=power
struct power_c_d_i { char c; double d; int i; };
#pragma options align=reset
CHECK(sizeof(struct power_c_d_i) == 16);
#pragma options align=natural
struct natural_c_d_i { char c; double d; int i; };
#pragma options align=reset
CHECK(__builtin_offsetof(struct natural_c_d_i, d) == 8);
CHECK(sizeof(struct natural_c_d_i) == 24);
#endif

#ifdef M68K
CHECK(_Alignof(int) == 2 && _Alignof(long) == 2 && _Alignof(void *) == 2);
CHECK(_Alignof(double) == 2 && _Alignof(long long) == 2);
CHECK(_Alignof(float) == 2);
CHECK(__builtin_offsetof(struct c_d_i, d) == 2);
CHECK(__builtin_offsetof(struct c_d_i, i) == 10);
CHECK(sizeof(struct c_d_i) == 14 && _Alignof(struct c_d_i) == 2);
CHECK(sizeof(struct c_i) == 6 && sizeof(struct c_s) == 4);
CHECK(sizeof(struct c_ll) == 10 && sizeof(struct c_f) == 6);
CHECK(sizeof(struct c_p) == 6 && sizeof(struct d_c) == 10);
CHECK(sizeof(struct c) == 1 && sizeof(struct c_c_c) == 3);

#pragma options align=mac68k
struct m68k_c_d_i { char c; double d; int i; };
#pragma options align=reset
CHECK(sizeof(struct m68k_c_d_i) == 14);
#endif
