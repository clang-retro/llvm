; 32-bit OS/2: __thread variables go through emulated TLS (the C library's
; __emutls_get_address) and C++ exceptions use SjLj function contexts.
; RUN: llc -mtriple=i386-pc-os2 < %s | FileCheck %s
; RUN: llc -mtriple=i386-pc-os2 -exception-model=sjlj < %s | FileCheck %s --check-prefix=EH

@x = thread_local global i32 1

define i32 @get() {
  %v = load i32, ptr @x
  ret i32 %v
}

; CHECK-LABEL: get:
; CHECK: pushl $__emutls_v.x
; CHECK: calll __emutls_get_address
; CHECK: __emutls_v.x:

declare void @g()
declare i32 @__gxx_personality_sj0(...)

define void @h() personality ptr @__gxx_personality_sj0 {
  invoke void @g() to label %ok unwind label %lp
ok:
  ret void
lp:
  %l = landingpad { ptr, i32 } cleanup
  resume { ptr, i32 } %l
}

; EH-LABEL: h:
; EH: calll _Unwind_SjLj_Register
; EH: calll _Unwind_SjLj_Unregister
; EH: calll _Unwind_SjLj_Resume
