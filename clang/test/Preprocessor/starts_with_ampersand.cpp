// RUN: %clang_cc1 -E %s -o - | FileCheck %s
// RUN: %clang_cc1 -E -verify=err -DERRORS %s -o /dev/null

// clang-retro: __starts_with_ampersand(tokens) is 1 when the macro-expanded
// tokens begin with '&', and 0 otherwise.

#ifndef __starts_with_ampersand
#error __starts_with_ampersand should be defined
#endif

#define AMP_FIRST(x) __starts_with_ampersand(x)
#define HANDLER &CMyApp::OnAbout

// CHECK: a: 1
a: AMP_FIRST(&CMyApp::OnAbout)
// CHECK: b: 0
b: AMP_FIRST(OnAbout)
// CHECK: c: 0
c: AMP_FIRST(CWinApp::OnFileNew)
// CHECK: d: 1
d: AMP_FIRST(HANDLER)
// CHECK: e: 0
e: AMP_FIRST((&x))
// CHECK: f: 1
f: AMP_FIRST(&f(a, (b), c))
// CHECK: g: 0
g: AMP_FIRST(a & b)
// CHECK: h: 0
h: __starts_with_ampersand()

#define CAT(a, b) CAT2(a, b)
#define CAT2(a, b) a##b
#define CALL_0(f) plain f
#define CALL_1(f) pointer f
// CHECK: i: plain OnAbout
i: CAT(CALL_, AMP_FIRST(OnAbout))(OnAbout)
// CHECK: j: pointer &CMyApp::OnAbout
j: CAT(CALL_, AMP_FIRST(&CMyApp::OnAbout))(&CMyApp::OnAbout)

#if __starts_with_ampersand(&x) && !__starts_with_ampersand(x)
// CHECK: k: in if
k: in if
#endif

#ifdef ERRORS
int l = __starts_with_ampersand; // err-error {{missing '(' after '__starts_with_ampersand'}}
#endif
