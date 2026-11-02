// RUN: %clang_cc1 -triple m68k-unknown-amigaos -fsyntax-only -verify %s
// expected-no-diagnostics

// Amiga compilers align everything of 2 bytes or more to 2.
struct S1 { char c; double d; int i; };
_Static_assert(sizeof(struct S1) == 14, "");
_Static_assert(_Alignof(double) == 2, "");
_Static_assert(_Alignof(long long) == 2, "");
_Static_assert(_Alignof(long) == 2, "");
_Static_assert(_Alignof(void *) == 2, "");
_Static_assert(sizeof(long double) == 8, "");
_Static_assert(sizeof(int) == 4, "");
_Static_assert(sizeof(void *) == 4, "");
