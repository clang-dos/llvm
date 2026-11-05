// RUN: %clang_cc1 -triple powerpc-apple-macosx10.0 -target-feature +altivec -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple powerpc-unknown-linux-gnu -target-feature +altivec -fsyntax-only -verify=linux %s

// Apple's gcc accepted the deprecated "vector long" on 32-bit PowerPC Mac OS
// X as "vector int", and Mac OS X 10.0's MachineExceptions.h uses it.
typedef vector unsigned long VUL; // expected-warning {{use of 'long' with '__vector' is deprecated}} \
                                  // linux-error {{cannot use 'long' with '__vector'}}
_Static_assert(sizeof(VUL) == 16, "");
_Static_assert(sizeof(((VUL){0})[0]) == 4, "");
