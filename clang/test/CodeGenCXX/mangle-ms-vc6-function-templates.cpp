// Visual C++ 6.0 and before mangle a specialization of a function template
// as a plain function, without its template arguments. msvcp60.dll and
// mfc42.dll export their function templates by these names. Visual C++
// 2002 (13.00) and later add the template arguments (??$name@args@...).

// RUN: %clang_cc1 -std=c++11 -fms-extensions -emit-llvm %s -o - -triple=i386-pc-win32 -fms-compatibility-version=12.00 | FileCheck %s --check-prefix=VC6
// RUN: %clang_cc1 -std=c++11 -fms-extensions -emit-llvm %s -o - -triple=i386-pc-win32 -fms-compatibility-version=13.00 | FileCheck %s --check-prefix=VC7
// RUN: %clang_cc1 -std=c++11 -fms-extensions -emit-llvm %s -o - -triple=i386-pc-win32 | FileCheck %s --check-prefix=VC7

template <class T> T twice(T x) { return x + x; }

namespace std {
template <class E> struct char_traits {};
template <class E, class Tr = char_traits<E> > struct basic_ostream {};
template <class Tr>
basic_ostream<char, Tr> &operator<<(basic_ostream<char, Tr> &os, const char *) {
  return os;
}
}

class CString {};
template <class T> void __stdcall ConstructElements(T *p, int n);
template <> void __stdcall ConstructElements<CString>(CString *p, int n);

struct Obj {
  template <class T> void put(T) {}
};

void use(std::basic_ostream<char> &os, CString *s, Obj &o) {
  twice(1);
  twice(2.0);
  os << "x";
  ConstructElements(s, 1);
  o.put('c');
}

// Class templates keep their arguments (?$...) in both.
// VC6-DAG: define linkonce_odr dso_local noundef i32 @"?twice@@YAHH@Z"
// VC6-DAG: define linkonce_odr dso_local noundef double @"?twice@@YANN@Z"
// VC6-DAG: define linkonce_odr dso_local noundef nonnull align 1 dereferenceable(1) ptr @"??6std@@YAAAU?$basic_ostream@DU?$char_traits@D@std@@@0@AAU10@PBD@Z"
// VC6-DAG: declare dso_local x86_stdcallcc void @"?ConstructElements@@YGXPAVCString@@H@Z"
// VC6-DAG: define linkonce_odr dso_local x86_thiscallcc void @"?put@Obj@@QAEXD@Z"

// VC7-DAG: define linkonce_odr dso_local noundef i32 @"??$twice@H@@YAHH@Z"
// VC7-DAG: define linkonce_odr dso_local noundef double @"??$twice@N@@YANN@Z"
// VC7-DAG: define linkonce_odr dso_local noundef nonnull align 1 dereferenceable(1) ptr @"??$?6U?$char_traits@D@std@@@std@@YAAAU?$basic_ostream@DU?$char_traits@D@std@@@0@AAU10@PBD@Z"
// VC7-DAG: declare dso_local x86_stdcallcc void @"??$ConstructElements@VCString@@@@YGXPAVCString@@H@Z"
// VC7-DAG: define linkonce_odr dso_local x86_thiscallcc void @"??$put@D@Obj@@QAEXD@Z"
