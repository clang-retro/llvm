; RUN: rm -rf %t && mkdir -p %t
; RUN: llc -mtriple=powerpc-ibm-aix -filetype=obj %s -o %t/a.o
; RUN: not xcoff-ld -o %t/a %t/a.o -bI:%S/Inputs/unix.exp 2>&1 | FileCheck %s
; RUN: not xcoff-ld -o %t/a %t/a.o %t/a.o -bnoentry -bI:%S/Inputs/unix.exp 2>&1 \
; RUN:   | FileCheck %s --check-prefix=DUP

; CHECK: error: undefined symbol: .missing
; CHECK: error: undefined symbol: __start

; DUP: error: duplicate symbol: .f

target datalayout = "E-m:a-p:32:32-Fi32-i64:64-n32"
target triple = "powerpc-ibm-aix"

declare i32 @missing()

define i32 @f() {
  %r = call i32 @missing()
  ret i32 %r
}
