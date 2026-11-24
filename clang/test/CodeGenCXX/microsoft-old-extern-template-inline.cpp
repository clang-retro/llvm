// Visual C++ 2003 and before ignore an explicit instantiation declaration
// of an inline function that is not dllimport, and emit the function where
// it is used. Their C++ library headers declare the inline operators of
// std::basic_string so, and msvcp60.dll does not export them.

// RUN: %clang_cc1 -std=c++11 -fms-extensions -emit-llvm %s -o - -triple=i386-pc-win32 -fms-compatibility-version=12.00 | FileCheck %s --check-prefix=OLD
// RUN: %clang_cc1 -std=c++11 -fms-extensions -emit-llvm %s -o - -triple=i386-pc-win32 -fms-compatibility-version=13.10 | FileCheck %s --check-prefix=VC7
// RUN: %clang_cc1 -std=c++11 -fms-extensions -emit-llvm %s -o - -triple=i386-pc-win32 -fms-compatibility-version=19.00 | FileCheck %s --check-prefix=NEW

template <class T> inline int plus(const T &a, int b) { return (int)a + b; }
template <class T> int notinline(const T &a) { return (int)a; }
template <class T> inline int imported(const T &a) { return (int)a * 2; }

extern template int plus<char>(const char &, int);
extern template int notinline<char>(const char &);
extern template __declspec(dllimport) int imported<char>(const char &);

int use(char c) { return plus(c, 1) + notinline(c) + imported(c); }

// The inline one is emitted, as an implicit instantiation would be.
// OLD-DAG: define linkonce_odr dso_local noundef i32 @"?plus@@YAHABDH@Z"
// VC7-DAG: define linkonce_odr dso_local noundef i32 @"??$plus@D@@YAHABDH@Z"
// NEW-DAG: declare dso_local noundef i32 @"??$plus@D@@YAHABDH@Z"
// A function that is not inline and a dllimport one stay declarations.
// OLD-DAG: declare dso_local noundef i32 @"?notinline@@YAHABD@Z"
// OLD-DAG: declare dllimport noundef i32 @"?imported@@YAHABD@Z"
// VC7-DAG: declare dso_local noundef i32 @"??$notinline@D@@YAHABD@Z"
// VC7-DAG: declare dllimport noundef i32 @"??$imported@D@@YAHABD@Z"
// NEW-DAG: declare dso_local noundef i32 @"??$notinline@D@@YAHABD@Z"
