; RUN: rm -rf %t && mkdir -p %t
; RUN: llc -mtriple=powerpc-ibm-aix -filetype=obj %s -o %t/a.o
; RUN: llc -mtriple=powerpc-ibm-aix -filetype=obj %S/Inputs/start.ll -o %t/start.o
; RUN: printf '* exported to the loader\nerrno\n' > %t/libc.exp
; RUN: xcoff-ld -o %t/a %t/start.o %t/a.o -bI:%S/Inputs/unix.exp -bE:%t/libc.exp
; RUN: llvm-readobj --section-headers --loader-section-symbols %t/a | FileCheck %s
; RUN: llvm-objdump -s -j .data %t/a | FileCheck %s --check-prefix=DATA

;; errno is exported (L_EXPORT | XTY_SD) from .data, with its storage
;; class XMC_RW; the entry point is L_ENTRY.
; CHECK:      Name: .bss
; CHECK-NEXT: PhysicalAddress: 0x200001B8
; CHECK:      Name: __start
; CHECK-NEXT: Virtual Address: 0x20000190
; CHECK-NEXT: SectionNum: 2
; CHECK-NEXT: SymbolType: 0x21
; CHECK:      Name: errno
; CHECK-NEXT: Virtual Address: 0x2000019C
; CHECK-NEXT: SectionNum: 2
; CHECK-NEXT: SymbolType: 0x11
; CHECK-NEXT: StorageClass: {{.*}} (0x5)
; CHECK-NEXT: ImportFileID: 0x0

;; The TOC holds the address of errno, then _end: the end of .bss.
; DATA:      Contents of section .data:
; DATA:      200001b0 200001b8 00000000

target datalayout = "E-m:a-p:32:32-Fi32-i64:64-n32"
target triple = "powerpc-ibm-aix"

@errno = global i32 0
@_end = external global i8

define ptr @main() {
  store i32 1, ptr @errno
  ret ptr @_end
}
