// RUN: %clang_cc1 -triple i386-pc-win32 -fms-extensions -Wunknown-pragmas -verify=ms -DMS %s
// RUN: %clang_cc1 -triple i386-pc-win32 -Wunknown-pragmas -verify=std %s
// ms-no-diagnostics

// With -fms-extensions, a macro in place of a pragma's name is expanded, as
// Microsoft's compiler does; without, the pragma is unknown.

#define PACK_ONE pack(1)
#pragma PACK_ONE // std-warning {{unknown pragma ignored}}
struct s { char c; int i; };
#ifdef MS
_Static_assert(sizeof(struct s) == 5, "packed");
#else
_Static_assert(sizeof(struct s) == 8, "not packed");
#endif
