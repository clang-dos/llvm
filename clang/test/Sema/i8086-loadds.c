// RUN: %clang_cc1 -triple i8086-pc-msdos -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -fsyntax-only -verify=x64 %s

int __attribute__((loadds)) f(void); // x64-warning {{unknown attribute 'loadds' ignored}}
int __attribute__((loadds)) v; // expected-warning {{'loadds' attribute only applies to functions}} \
                               // x64-warning {{unknown attribute 'loadds' ignored}}
int __attribute__((loadds(1))) g(void); // expected-error {{'loadds' attribute takes no arguments}} \
                                        // x64-warning {{unknown attribute 'loadds' ignored}}
