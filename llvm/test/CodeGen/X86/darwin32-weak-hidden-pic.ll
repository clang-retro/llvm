; RUN: llc -mtriple=i386-apple-macosx10.4 -relocation-model=pic < %s \
; RUN:   | FileCheck %s --check-prefix=PIC
; RUN: llc -mtriple=i386-apple-macosx10.4 -relocation-model=dynamic-no-pic \
; RUN:   < %s | FileCheck %s --check-prefix=DNP

; ld64 stops with "bad codegen, pointer diff in X to global weak symbol Y"
; when X subtracts the PIC base from a global weak symbol. A hidden weak
; definition can merge with a default-visibility copy from another object
; (for example an inline variable of libc++ built with -fvisibility=hidden,
; and a program built without it), and the merged symbol is then global.
; So 32-bit Mach-O PIC code reads weak definitions through a non-lazy
; pointer, also when they are hidden. Strong hidden definitions stay direct.

@weak_hidden_table = linkonce_odr hidden constant [2 x i32] [i32 1, i32 10]
@weak_hidden_var = weak hidden global i32 5
@strong_hidden_var = hidden global i32 7

define i32 @read_weak_hidden_table(i32 %i) nounwind {
; PIC-LABEL: _read_weak_hidden_table:
; PIC:       movl L_weak_hidden_table$non_lazy_ptr-L0$pb(%{{e[a-d]x}}), %{{e[a-d]x}}
; PIC-NOT:   _weak_hidden_table-L0$pb
; DNP-LABEL: _read_weak_hidden_table:
; DNP:       _weak_hidden_table(,%{{e[a-d]x}},4)
  %p = getelementptr [2 x i32], ptr @weak_hidden_table, i32 0, i32 %i
  %v = load i32, ptr %p
  ret i32 %v
}

define ptr @addr_weak_hidden_var() nounwind {
; PIC-LABEL: _addr_weak_hidden_var:
; PIC:       movl L_weak_hidden_var$non_lazy_ptr-L1$pb(%eax), %eax
; DNP-LABEL: _addr_weak_hidden_var:
; DNP:       movl $_weak_hidden_var, %eax
  ret ptr @weak_hidden_var
}

define ptr @addr_strong_hidden_var() nounwind {
; PIC-LABEL: _addr_strong_hidden_var:
; PIC:       leal _strong_hidden_var-L2$pb(%eax), %eax
; DNP-LABEL: _addr_strong_hidden_var:
; DNP:       movl $_strong_hidden_var, %eax
  ret ptr @strong_hidden_var
}

; PIC:      .section __IMPORT,__pointers,non_lazy_symbol_pointers
; PIC-DAG:  L_weak_hidden_table$non_lazy_ptr:
; PIC-DAG:  L_weak_hidden_var$non_lazy_ptr:
; PIC-NOT:  L_strong_hidden_var$non_lazy_ptr:
