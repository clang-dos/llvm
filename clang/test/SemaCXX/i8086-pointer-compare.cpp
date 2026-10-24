// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi large -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi small -fsyntax-only -verify %s
// expected-no-diagnostics

// With far data, a default pointer and a __far one convert to each other,
// so they compare, and the composite type is the default pointer. A near
// pointer converts to either. With near data the composite is far.

char *dp;
char __far *fp;
char __near *np;

bool a = dp == fp;
bool b = fp != dp;
bool c = np == fp;
auto d = true ? dp : fp;
