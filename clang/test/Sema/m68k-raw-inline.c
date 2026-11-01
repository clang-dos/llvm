// RUN: %clang_cc1 -triple m68k-apple-macos -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -fsyntax-only -verify=x86 -DNOT_M68K %s

#ifdef NOT_M68K
void elsewhere(void) __attribute__((raw_inline(0xA86E))); // x86-warning {{unknown attribute 'raw_inline' ignored}}
#else
pascal void ok(void) __attribute__((raw_inline(0xA86E, 0x0001)));
void too_big(void) __attribute__((raw_inline(0x10000))); // expected-error {{'raw_inline' attribute requires integer constant between 0 and 65535 inclusive}}
void none(void) __attribute__((raw_inline)); // expected-error {{'raw_inline' attribute takes at least 1 argument}}
int var __attribute__((raw_inline(1))); // expected-warning {{'raw_inline' attribute only applies to functions}}
#endif
