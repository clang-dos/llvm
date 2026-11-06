// RUN: %clang_cc1 -triple i8086-pc-msdos -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple i8086-pc-os2 -fsyntax-only -verify %s
// expected-no-diagnostics

// __far before a later declarator in a list qualifies the type specifier
// for that declarator, as it does in the first one (OS/2 and Windows
// headers: typedef struct _R {...} R, FAR *PR;).

typedef struct _R { short a; } R, __far *PR, __far *__far *PPR;
void takes_pr(PR p);
void takes_r(R r);
_Static_assert(sizeof(R) == 2, "");
_Static_assert(sizeof(PR) == 4, "");
_Static_assert(sizeof(PPR) == 4, "");
_Static_assert(sizeof(*(PPR)0) == 4, "");

int x, __far y, __far *z;
_Static_assert(sizeof(z) == 4, "");
_Static_assert(sizeof(&y) == 4, "");
_Static_assert(sizeof(&x) == 2, "");

int a, __far fn(void);
_Static_assert(sizeof(&fn) == 4, "");
