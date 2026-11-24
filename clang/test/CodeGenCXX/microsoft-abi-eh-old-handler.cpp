// Visual C++ 2003 and before use __CxxFrameHandler, the C++ handler of
// msvcrt.dll to msvcr71.dll, for 32-bit x86. Visual C++ 2005 and later use
// __CxxFrameHandler3. x64 always uses __CxxFrameHandler3.

// RUN: %clang_cc1 -std=c++11 -emit-llvm %s -o - -triple=i386-pc-win32 -fexceptions -fcxx-exceptions -fms-compatibility-version=12.00 | FileCheck %s --check-prefix=OLD
// RUN: %clang_cc1 -std=c++11 -emit-llvm %s -o - -triple=i386-pc-win32 -fexceptions -fcxx-exceptions -fms-compatibility-version=13.10 | FileCheck %s --check-prefix=OLD
// RUN: %clang_cc1 -std=c++11 -emit-llvm %s -o - -triple=i386-pc-win32 -fexceptions -fcxx-exceptions -fms-compatibility-version=14.00 | FileCheck %s --check-prefix=NEW
// RUN: %clang_cc1 -std=c++11 -emit-llvm %s -o - -triple=i386-pc-win32 -fexceptions -fcxx-exceptions | FileCheck %s --check-prefix=NEW
// RUN: %clang_cc1 -std=c++11 -emit-llvm %s -o - -triple=x86_64-pc-win32 -fexceptions -fcxx-exceptions -fms-compatibility-version=12.00 | FileCheck %s --check-prefix=NEW

void may_throw();
void handle(int);

// OLD-LABEL: define dso_local void @"?catch_int@@YAXXZ"()
// OLD-SAME: personality ptr @__CxxFrameHandler{{ }}
// NEW-LABEL: define dso_local void @"?catch_int@@YAXXZ"()
// NEW-SAME: personality ptr @__CxxFrameHandler3
void catch_int() {
  try {
    may_throw();
  } catch (int e) {
    handle(e);
  }
}

struct S {
  ~S();
};

// OLD-LABEL: define dso_local void @"?cleanup@@YAXXZ"()
// OLD-SAME: personality ptr @__CxxFrameHandler{{ }}
// OLD: cleanuppad
// NEW-LABEL: define dso_local void @"?cleanup@@YAXXZ"()
// NEW-SAME: personality ptr @__CxxFrameHandler3
void cleanup() {
  S s;
  may_throw();
}
