// A pointer qualified with the memory model's own kind of pointer is a plain
// pointer: __near in small and medium, __far in compact and large, __huge in
// huge. As in Microsoft C, `char __far *` and `char *` are one type in the
// large model, for overloads, templates, delete and name mangling.
// RUN: %clang_cc1 -triple i8086-pc-win16 -target-abi large -fsyntax-only -verify=far %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi compact -fsyntax-only -verify=far %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi small -fsyntax-only -verify=near %s
// RUN: %clang_cc1 -triple i8086-pc-msdos -target-abi huge -fsyntax-only -verify=huge %s
// RUN: %clang_cc1 -triple i8086-pc-win16 -target-abi large -emit-llvm -DCODEGEN -o - %s | FileCheck %s

template <class T, class U> struct same { static const bool value = false; };
template <class T> struct same<T, T> { static const bool value = true; };

typedef char __far *LPSTR;
typedef char __near *PSTR;
typedef const int __far &FREF;

#ifndef CODEGEN
static_assert(same<LPSTR, char *>::value, "far");   // near-error {{static assertion failed}} huge-error {{static assertion failed}}
static_assert(same<PSTR, char *>::value, "near");   // far-error {{static assertion failed}} huge-error {{static assertion failed}}
static_assert(same<FREF, const int &>::value, "&"); // near-error {{static assertion failed}} huge-error {{static assertion failed}}
static_assert(!same<char __huge *, char *>::value, "huge"); // huge-error {{static assertion failed}}
#endif

// One function, declared twice in the far-data models; two overloads in
// the near-data ones.
void take(char *);
void take(LPSTR);

// delete takes the model's own pointers.
void drop(LPSTR p) {
  delete[] p; // near-error {{'delete' cannot delete objects of type 'char' in address space '2'}} huge-error {{'delete' cannot delete objects of type 'char' in address space '2'}}
}

// CHECK: define {{.*}}void @_Z4dropPc(ptr addrspace(2) noundef %p)
