; RUN: rm -rf %t && mkdir -p %t
; RUN: llc -mtriple=powerpc-ibm-aix -filetype=obj %s -o %t/hello.o
; RUN: llc -mtriple=powerpc-ibm-aix -filetype=obj %S/Inputs/start.ll -o %t/start.o
; RUN: llc -mtriple=powerpc-ibm-aix -filetype=obj %S/Inputs/lib1.ll -o %t/lib1.o
; RUN: llc -mtriple=powerpc-ibm-aix -filetype=obj %S/Inputs/unused.ll -o %t/unused.o
; RUN: llvm-ar --format=bigarchive rcs %t/libt.a %t/lib1.o %t/unused.o
; RUN: xcoff-ld -o %t/hello %t/start.o %t/hello.o -L%t -lt -bI:%S/Inputs/unix.exp
; RUN: llvm-readobj --file-headers --auxiliary-header --section-headers %t/hello \
; RUN:   | FileCheck %s --check-prefix=HDR
; RUN: llvm-readobj --loader-section-header --loader-section-symbols \
; RUN:   --loader-section-relocations %t/hello | FileCheck %s --check-prefix=LDR
; RUN: llvm-objdump -s -j .loader %t/hello | FileCheck %s --check-prefix=IMP
; RUN: llvm-objdump -s -j .data %t/hello | FileCheck %s --check-prefix=DATA
; RUN: llvm-objdump -d %t/hello | FileCheck %s --check-prefix=DIS

;; The program is an executable that the system loader binds to /unix.
; HDR:      Magic: 0x1DF
; HDR:      NumberOfSections: 4
; HDR:      Flags: 0x1007
; HDR:      AuxiliaryHeader {
; HDR-NEXT:   Magic: 0x10B
; HDR-NEXT:   Version: 0x1
; HDR-NEXT:   Size of .text section: 0x114
; HDR-NEXT:   Size of .data section: 0x38
; HDR-NEXT:   Size of .bss section: 0x0
; HDR-NEXT:   Entry point address: 0x20000218
; HDR-NEXT:   .text section start address: 0x10000100
; HDR-NEXT:   .data section start address: 0x20000218
; HDR-NEXT:   TOC anchor address: 0x20000240
; HDR-NEXT:   Section number of entryPoint: 2
; HDR-NEXT:   Section number of .text: 1
; HDR-NEXT:   Section number of .data: 2
; HDR-NEXT:   Section number of TOC: 2
; HDR-NEXT:   Section number of loader data: 4
; HDR-NEXT:   Section number of .bss: 3
; HDR:        Module type: 0x314C
; HDR:        Name: .text
; HDR-NEXT:   PhysicalAddress: 0x10000100
; HDR-NEXT:   VirtualAddress: 0x10000100
; HDR-NEXT:   Size: 0x114
; HDR-NEXT:   RawDataOffset: 0x100
; HDR:        Name: .data
; HDR-NEXT:   PhysicalAddress: 0x20000218
; HDR-NEXT:   VirtualAddress: 0x20000218
; HDR-NEXT:   Size: 0x38
; HDR-NEXT:   RawDataOffset: 0x218
; HDR:        Name: .bss
; HDR:        Name: .loader
; HDR:        Type: STYP_LOADER (0x1000)

;; The entry point, then the imported system calls with storage class
;; XMC_SV (0x8) from import file 1, which is /unix.  Every address in .data
;; has a loader relocation; the TOC entries of the glink code point at the
;; imports.
; LDR:      NumberOfSymbolEntries: 3
; LDR-NEXT: NumberOfRelocationEntries: 10
; LDR-NEXT: LengthOfImportFileIDStringTable: 24
; LDR-NEXT: NumberOfImportFileIDs: 2
; LDR:      Name: __start
; LDR-NEXT: Virtual Address: 0x20000218
; LDR-NEXT: SectionNum: 2
; LDR-NEXT: SymbolType: 0x21
; LDR-NEXT: StorageClass: {{.*}} (0xA)
; LDR-NEXT: ImportFileID: 0x0
; LDR:      Name: _exit
; LDR-NEXT: Virtual Address: 0x0
; LDR-NEXT: SectionNum: 0
; LDR-NEXT: SymbolType: 0x40
; LDR-NEXT: StorageClass: {{.*}} (0x8)
; LDR-NEXT: ImportFileID: 0x1
; LDR:      Name: kwrite
; LDR-NEXT: Virtual Address: 0x0
; LDR-NEXT: SectionNum: 0
; LDR-NEXT: SymbolType: 0x40
; LDR-NEXT: StorageClass: {{.*}} (0x8)
; LDR-NEXT: ImportFileID: 0x1
; LDR:      0x20000218 0x1f00 (R_POS)       2    .text (0)
; LDR-NEXT: 0x2000021c 0x1f00 (R_POS)       2    .data (1)
; LDR-NEXT: 0x20000224 0x1f00 (R_POS)       2    .text (0)
; LDR-NEXT: 0x20000228 0x1f00 (R_POS)       2    .data (1)
; LDR-NEXT: 0x20000234 0x1f00 (R_POS)       2    .text (0)
; LDR-NEXT: 0x20000238 0x1f00 (R_POS)       2    .data (1)
; LDR-NEXT: 0x20000240 0x1f00 (R_POS)       2    .text (0)
; LDR-NEXT: 0x20000244 0x1f00 (R_POS)       2    .data (1)
; LDR-NEXT: 0x20000248 0x1f00 (R_POS)       2    _exit (4)
; LDR-NEXT: 0x2000024c 0x1f00 (R_POS)       2    kwrite (5)

;; The import IDs: the default library path, then /unix.
; IMP:      Contents of section .loader:
; IMP:      2f757372 2f6c6962 3a2f6c69 62000000  /usr/lib:/lib...
; IMP-NEXT: 2f00756e 69780000                    /.unix..

;; Descriptors hold the entry point and the TOC anchor; the TOC holds the
;; string, the counter, and zero words that the loader fills in.
; DATA:      Contents of section .data:
; DATA-NEXT: 20000218 10000100 20000240 00000000 10000140
; DATA-NEXT: 20000228 20000240 00000000 00000007 100001a0
; DATA-NEXT: 20000238 20000240 00000000 10000190 20000230
; DATA-NEXT: 20000248 00000000 00000000

;; A call to an imported function goes through glink code, which loads the
;; descriptor from the TOC; the TOC is restored after the call.  A call within
;; the program keeps its nop.  The unused archive member is not linked.
; DIS:      10000114: 48 00 00 b9 bl 0x100001cc
; DIS-NEXT: 10000118: 80 41 00 14 lwz 2, 20(1)
; DIS:      10000148: 80 82 00 00 lwz 4, 0(2)
; DIS:      10000158: 48 00 00 99 bl 0x100001f0
; DIS-NEXT: 1000015c: 80 41 00 14 lwz 2, 20(1)
; DIS-NEXT: 10000160: 48 00 00 41 bl 0x100001a0
; DIS-NEXT: 10000164: 60 00 00 00 nop
; DIS:      100001a0: 80 82 00 04 lwz 4, 4(2)
; DIS:      100001cc: 81 82 00 08 lwz 12, 8(2)
; DIS-NEXT: 100001d0: 90 41 00 14 stw 2, 20(1)
; DIS-NEXT: 100001d4: 80 0c 00 00 lwz 0, 0(12)
; DIS-NEXT: 100001d8: 80 4c 00 04 lwz 2, 4(12)
; DIS-NEXT: 100001dc: 7c 09 03 a6 mtctr 0
; DIS-NEXT: 100001e0: 4e 80 04 20 bctr
; DIS:      100001f0: 81 82 00 0c lwz 12, 12(2)

target datalayout = "E-m:a-p:32:32-Fi32-i64:64-n32"
target triple = "powerpc-ibm-aix"

@msg = private constant [3 x i8] c"hi\0A"

declare i32 @kwrite(i32, ptr, i32)
declare i32 @helper(i32)

define i32 @main() {
  %w = call i32 @kwrite(i32 1, ptr @msg, i32 3)
  %h = call i32 @helper(i32 %w)
  ret i32 %h
}
