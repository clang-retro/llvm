; Classic Mac OS has no thread pointer, so thread-local variables use
; emulated TLS by default: each access calls __emutls_get_address with the
; variable's control record.

; RUN: llc -mtriple=m68k-apple-macos < %s | FileCheck %s

@tv = thread_local global i32 5, align 4

define i32 @get() {
; CHECK-LABEL: get:
; CHECK: jsr __emutls_get_address
entry:
  %v = load i32, ptr @tv, align 4
  ret i32 %v
}

; CHECK: __emutls_v.tv:
; CHECK: __emutls_t.tv:
; CHECK-NEXT: .long 5
