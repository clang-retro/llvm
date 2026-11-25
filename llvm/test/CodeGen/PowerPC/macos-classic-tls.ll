; Classic Mac OS has no thread pointer, so thread-local variables use
; emulated TLS by default: each access calls __emutls_get_address with the
; variable's control record.

; RUN: llc -mtriple=powerpc-apple-macos9.2 < %s | FileCheck %s

@tv = thread_local global i32 5, align 4

define i32 @get() {
; CHECK-LABEL: .get:
; CHECK: bl .__emutls_get_address
entry:
  %v = load i32, ptr @tv, align 4
  ret i32 %v
}

; The variable itself is not emitted; its control record and initial
; value are.
; CHECK-NOT: tv[TL]
; CHECK: .csect __emutls_v.tv[RW],2
; CHECK: .vbyte 4, __emutls_t.tv[RO]
; CHECK: .csect __emutls_t.tv[RO],2
; CHECK: .vbyte 4, 5
