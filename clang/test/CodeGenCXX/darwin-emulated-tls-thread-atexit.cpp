// RUN: %clang_cc1 -triple powerpc-apple-macosx10.4 -femulated-tls -std=c++11 -emit-llvm -o - %s | FileCheck %s --check-prefix=EMUTLS
// RUN: %clang_cc1 -triple x86_64-apple-macosx10.15 -std=c++11 -emit-llvm -o - %s | FileCheck %s --check-prefix=TLV

// With emulated TLS, Darwin has no _tlv_atexit for the destructor of a
// thread_local variable: libc++abi's __cxa_thread_atexit registers it.

struct S {
  ~S();
};

thread_local S s;

// EMUTLS: call i32 @__cxa_thread_atexit(
// EMUTLS-NOT: _tlv_atexit
// TLV: call i32 @_tlv_atexit(
