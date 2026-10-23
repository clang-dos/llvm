// The i8086 __near, __far and __huge qualifiers: near pointers convert to
// far and huge ones, and far and huge to each other; far or huge to near
// needs a cast.
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi small -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi compact -fsyntax-only -verify=expected,fardata %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge -fsyntax-only -verify=expected,fardata %s

_Static_assert(sizeof(char __near *) == 2, "");
_Static_assert(sizeof(char __far *) == 4, "");
_Static_assert(sizeof(char __huge *) == 4, "");

int __far table[100];
char __near *np;
char __far *fp;
char __huge *hp;

void f(char *p) {
  fp = np;
  hp = np;
  hp = fp;
  fp = hp;
  np = (char __near *)fp;
  np = fp; // expected-error {{changes address space of pointer}}
  np = hp; // expected-error {{changes address space of pointer}}
  fp = p;
  np = p; // fardata-error {{changes address space of pointer}}
}

// On functions they make the function near or far, as in Open Watcom: the
// qualifier of the result type, or of a function pointer's pointee.
int __far ff(int);
int * __far ffp(void);
char __far *nfp(void); // A near function (in small) returning a far pointer.
int __near nf(int);
int (__far *pff)(int) = ff;
int __far (*pff2)(int) = ff;
int (__near *pnf)(int) = nf;
_Static_assert(sizeof(pff) == 4, "");
_Static_assert(sizeof(pnf) == 2, "");
int (__far *bad1)(int) = nf; // expected-error {{changes address space of pointer}}
int (__near *bad2)(int) = ff; // expected-error {{changes address space of pointer}}
void g(void) {
  char __far *c = nfp();
  int *i = ffp();
  (void)c;
  (void)i;
}
