# REQUIRES: x86

# A long-format import library from LINK 5.0 and before (as the msvcrt.lib of
# Visual C++ 5.0). Its members all have the name of the DLL. The import
# descriptor starts the lookup and address tables with empty .idata$4 and
# .idata$5 sections, and the "\x7f<DLL>_NULL_THUNK_DATA" object ends them.
# The members come in the order that the link loads them: the import first,
# then the descriptor that it refers to. The directory entries in .idata$2
# and .idata$3 have an alignment of 1.

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: mkdir imp head thunk null
# RUN: llvm-mc -triple=i686-windows-msvc -filetype=obj imp.s -o imp/foo.dll
# RUN: llvm-mc -triple=i686-windows-msvc -filetype=obj head.s -o head/foo.dll
# RUN: llvm-mc -triple=i686-windows-msvc -filetype=obj thunk.s -o thunk/foo.dll
# RUN: llvm-mc -triple=i686-windows-msvc -filetype=obj null.s -o null/foo.dll
# RUN: llvm-ar qc foo.lib imp/foo.dll thunk/foo.dll head/foo.dll null/foo.dll
# RUN: llvm-ar s foo.lib
# RUN: llvm-mc -triple=i686-windows-msvc -filetype=obj main.s -o main.obj
# RUN: lld-link -out:main.exe -entry:main -subsystem:console -safeseh:no main.obj foo.lib
# RUN: llvm-readobj --coff-imports main.exe | FileCheck %s
# RUN: llvm-readobj --file-headers main.exe | FileCheck --check-prefix=DIR %s

# CHECK:      Import {
# CHECK-NEXT:   Name: foo.dll
# CHECK-NEXT:   ImportLookupTableRVA:
# CHECK-NEXT:   ImportAddressTableRVA:
# CHECK-NEXT:   Symbol: func (1)
# CHECK-NEXT: }

# The import directory is aligned.
# DIR: ImportTableRVA: 0x{{[0-9A-F]*[048C]$}}

#--- main.s
        .text
        .globl _main
_main:
        calll _func
        retl
# One byte of read-only data before the import directory.
        .section .idata$1,"dr"
        .byte 1

#--- imp.s
        .text
        .globl _func
_func:
        jmpl *__imp__func
        .section .idata$5,"dr"
        .p2align 2
        .globl __imp__func
__imp__func:
        .rva hint
        .section .idata$4,"dr"
        .p2align 2
        .rva hint
        .section .idata$6,"dr"
        .p2align 1
hint:
        .short 1
        .asciz "func"
        .globl __IMPORT_DESCRIPTOR_foo

#--- head.s
        .section .idata$2,"dr"
        .globl __IMPORT_DESCRIPTOR_foo
__IMPORT_DESCRIPTOR_foo:
        .rva lookups
        .long 0
        .long 0
        .rva dllname
        .rva addresses
        .section .idata$4,"dr"
lookups:
        .section .idata$5,"dr"
addresses:
        .section .idata$6,"dr"
        .p2align 1
dllname:
        .asciz "foo.dll"
        .globl __NULL_IMPORT_DESCRIPTOR
        .globl "\177foo_NULL_THUNK_DATA"

#--- thunk.s
        .section .idata$5,"dr"
        .p2align 2
        .globl "\177foo_NULL_THUNK_DATA"
"\177foo_NULL_THUNK_DATA":
        .long 0
        .section .idata$4,"dr"
        .p2align 2
        .long 0

#--- null.s
        .section .idata$3,"dr"
        .globl __NULL_IMPORT_DESCRIPTOR
__NULL_IMPORT_DESCRIPTOR:
        .zero 20
