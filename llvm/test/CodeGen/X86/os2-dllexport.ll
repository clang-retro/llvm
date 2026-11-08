; OS/2 (LX) DLLs: each exported global gets an -export: directive in a
; .drectve note section, which JWlink reads from ELF objects.
; RUN: llc -mtriple=i386-pc-os2 < %s | FileCheck %s

define dllexport void @f() {
  ret void
}

define void @g() {
  ret void
}

@v = dllexport global i32 1

declare dllexport void @undefined()

; CHECK: .section .drectve,"",@note
; CHECK-NEXT: .ascii " -export:f -export:v"
