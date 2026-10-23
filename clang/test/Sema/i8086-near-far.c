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
