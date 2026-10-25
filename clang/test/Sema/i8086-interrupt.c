// RUN: %clang_cc1 -triple i8086-pc-msdos -fsyntax-only -verify %s

struct regs { unsigned r[15]; };
struct big { unsigned r[16]; };

void __interrupt a(void) {}
void __interrupt b(struct regs r) {}
void __interrupt c(unsigned es, unsigned ds) {}
int __interrupt d(void) { return 0; } // expected-error {{an '__interrupt' function must return 'void'}}
void __interrupt e(struct big r) {}   // expected-error {{an '__interrupt' function must take a structure of the saved registers}}
void __interrupt f(long x) {}         // expected-error {{an '__interrupt' function must take a structure of the saved registers}}

// Interrupt functions are far, so their address is a far function pointer.
void (__interrupt __far *p)() = a;
void (__interrupt *p2)(void) = a;
void (__far *q)(void) = a; // expected-error {{incompatible function pointer types}}
