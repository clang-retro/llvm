// Visual C++ 2002 and 2003 (13.00 and 13.10) link each function template
// specialization that a module uses but does not define to its Visual C++
// 6.0 name with /alternatename, so that they can call msvcp60.dll and
// mfc42.dll, which export function templates by those names.

// RUN: %clang_cc1 -std=c++11 -fms-extensions -emit-llvm %s -o - -triple=i386-pc-win32 -fms-compatibility-version=13.10 | FileCheck %s
// RUN: %clang_cc1 -std=c++11 -fms-extensions -emit-llvm %s -o - -triple=i386-pc-win32 -fms-compatibility-version=13.00 | FileCheck %s
// RUN: %clang_cc1 -std=c++11 -fms-extensions -emit-llvm %s -o - -triple=i386-pc-win32 -fms-compatibility-version=12.00 | FileCheck %s --check-prefix=NONE
// RUN: %clang_cc1 -std=c++11 -fms-extensions -emit-llvm %s -o - -triple=i386-pc-win32 -fms-compatibility-version=14.00 | FileCheck %s --check-prefix=NONE

class CString {};
template <class T> void __stdcall ConstructElements(T *p, int n);
template <> void __stdcall ConstructElements<CString>(CString *p, int n);
template <class T> __declspec(dllimport) int imported(T *p);
template <class T> int undefined(T a);
template <class T> int defined(T a) { return (int)a; }
struct S {
  template <class T> int m(T);
};

int use(CString *s, S &o) {
  ConstructElements(s, 1);
  return imported(s) + undefined(1) + defined(2) + o.m('c');
}

// CHECK: !llvm.linker.options = !{
// CHECK-DAG: !{!"/alternatename:??$ConstructElements@VCString@@@@YGXPAVCString@@H@Z=?ConstructElements@@YGXPAVCString@@H@Z"}
// CHECK-DAG: !{!"/alternatename:__imp_??$imported@VCString@@@@YAHPAVCString@@@Z=__imp_?imported@@YAHPAVCString@@@Z"}
// CHECK-DAG: !{!"/alternatename:??$undefined@H@@YAHH@Z=?undefined@@YAHH@Z"}
// CHECK-DAG: !{!"/alternatename:??$m@D@S@@QAEHD@Z=?m@S@@QAEHD@Z"}
// A specialization that the module defines has no alias.
// CHECK-NOT: /alternatename:??$defined

// NONE-NOT: /alternatename
