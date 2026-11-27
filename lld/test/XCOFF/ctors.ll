; Constructors and destructors (__sinit*/__sterm* descriptors) go into the
; tables between __init_array_start/__init_array_end and
; __fini_array_start/__fini_array_end, in priority order, each with a
; loader relocation.
; RUN: rm -rf %t && mkdir -p %t
; RUN: llc -mtriple=powerpc-ibm-aix -filetype=obj %S/Inputs/start.ll -o %t/start.o
; RUN: llc -mtriple=powerpc-ibm-aix -filetype=obj %s -o %t/a.o
; RUN: xcoff-ld -o %t/a %t/start.o %t/a.o -bI:%S/Inputs/unix.exp
; RUN: llvm-readobj --loader-section-relocations %t/a | FileCheck %s
; RUN: llvm-objdump -s --section=.data %t/a | FileCheck %s --check-prefix=DATA

;; The descriptors of late, early and fini are at 0x2000020c, 0x20000218
;; and 0x20000224. The init table at 0x2000023c holds early (priority 101)
;; then late; the fini table at 0x20000244 holds fini.
; DATA:      20000230 100001a0 20000248 00000000 20000218
; DATA-NEXT: 20000240 2000020c 20000224

; CHECK:      0x2000023c 0x1f00 (R_POS) 2 .data (1)
; CHECK-NEXT: 0x20000240 0x1f00 (R_POS) 2 .data (1)
; CHECK-NEXT: 0x20000244 0x1f00 (R_POS) 2 .data (1)

target datalayout = "E-m:a-p:32:32-Fi32-i64:64-n32-f64:32:64"
target triple = "powerpc-ibm-aix"

@llvm.global_ctors = appending global [2 x { i32, ptr, ptr }] [
  { i32, ptr, ptr } { i32 65535, ptr @late, ptr null },
  { i32, ptr, ptr } { i32 101, ptr @early, ptr null }]
@llvm.global_dtors = appending global [1 x { i32, ptr, ptr }] [
  { i32, ptr, ptr } { i32 65535, ptr @fini, ptr null }]

@__init_array_start = external global [0 x ptr]
@__init_array_end = external global [0 x ptr]
@__fini_array_start = external global [0 x ptr]
@__fini_array_end = external global [0 x ptr]

define internal void @late() { ret void }
define internal void @early() { ret void }
define internal void @fini() { ret void }

define i32 @main() {
  %a = ptrtoint ptr @__init_array_start to i32
  %b = ptrtoint ptr @__init_array_end to i32
  %c = ptrtoint ptr @__fini_array_start to i32
  %d = ptrtoint ptr @__fini_array_end to i32
  %n = sub i32 %b, %a
  %m = sub i32 %d, %c
  %r = add i32 %n, %m
  ret i32 %r
}
