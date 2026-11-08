// 32-bit OS/2: __export (Open Watcom), _Export (VisualAge C++) and
// dllexport mark a DLL's exports.
// RUN: %clang_cc1 -triple i386-pc-os2 -emit-llvm -o - %s | FileCheck %s

__export int a(void) { return 1; }
int _Export b(void) { return 2; }
__attribute__((dllexport)) int c;
int d(void) { return 3; }

// CHECK: @c = dllexport global i32 0
// CHECK: define {{.*}}dllexport i32 @a()
// CHECK: define {{.*}}dllexport i32 @b()
// CHECK: define {{.*}}i32 @d()
