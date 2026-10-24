// __near, __far and __huge on i8086 member functions: before the name they
// choose the call, after the parameter list they qualify `this`. A virtual
// function, or one a member pointer points to, uses the memory model's call.
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi small -fsyntax-only -verify=expected,nearcode %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi medium -fsyntax-only -verify=expected,farcode %s

struct S {
  int x;
  int __far farcall();
  int __near nearcall();
  int farthis() __far;
  int nearthis() __near;
  int both() const __far;
  static int st() __far; // A far function: a static member has no `this`.
  S() __far;
  ~S() __far;
};

// A definition keeps the call of the declaration.
int S::farcall() { return x; }
int __near S::nearcall() { return x; }
int __far S::st() { return 0; }

void use(S *p, S __far *fp, S __near *np) {
  S::st();
  p->farthis();
  fp->farthis();
  np->farthis();
  p->nearthis();
  fp->nearthis(); // expected-error {{cannot initialize object parameter}}
  fp->both();
}

int f();
int __far f(); // nearcode-error {{conflicting types for 'f'}} nearcode-note@-1 {{previous declaration}}

struct V {
  virtual int __far vf(); // nearcode-error {{virtual function 'vf' cannot be __far in this memory model}}
  virtual int __near vn(); // farcode-error {{virtual function 'vn' cannot be __near in this memory model}}
  virtual int vthis() __far;
};
struct W : V {
  int __near vthis() __far; // farcode-error {{virtual function 'vthis' cannot be __near in this memory model}}
};

int (__far S::*mp1)(); // nearcode-error {{pointer to member function cannot point to a __far function}}
int (__near S::*mp2)(); // farcode-error {{pointer to member function cannot point to a __near function}}
void g() {
  (void)&S::farcall; // nearcode-error {{pointer to member function cannot point to a __far function}}
  (void)&S::nearcall; // farcode-error {{pointer to member function cannot point to a __near function}}
}
