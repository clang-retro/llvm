// RUN: %clang_cc1 -fsyntax-only -verify -isystem %S/Inputs/wchar-t-typedef %s
// RUN: %clang_cc1 -fsyntax-only -verify=sys -Wsystem-headers -isystem %S/Inputs/wchar-t-typedef %s

// Old C headers declare 'typedef int wchar_t;' with no C++ guard. In a
// system header clang ignores it, as GCC did; wchar_t stays the built-in type.
#include <oldlib.h>
// sys-warning@oldlib.h:2 {{redeclaration of C++ built-in type 'wchar_t' in a system header ignored}}

static_assert(!__is_same(wchar_t, int), "wchar_t is still distinct");
wchar_t w = L'x';

// Outside a system header it is still an error.
typedef int wchar_t; // expected-error {{cannot combine with previous 'int' declaration specifier}} \
                     // expected-warning {{typedef requires a name}} \
                     // sys-error {{cannot combine with previous 'int' declaration specifier}} \
                     // sys-warning {{typedef requires a name}}
