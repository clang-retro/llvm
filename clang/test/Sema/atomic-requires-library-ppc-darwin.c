// RUN: %clang_cc1 %s -triple=powerpc-apple-macosx10.4 -verify
// expected-no-diagnostics

// On 32-bit PowerPC Mac OS X, the compiler-rt builtins (atomic.c) supply
// the atomic library functions, so a large atomic load or store is not an
// error.

struct foo {
  int big[128];
};

_Atomic(struct foo) bigAtomic;

void structAtomicStore(void) {
  struct foo f = {0};
  __c11_atomic_store(&bigAtomic, f, 5);
}

void structAtomicLoad(void) {
  struct foo f = __c11_atomic_load(&bigAtomic, 5);
}
