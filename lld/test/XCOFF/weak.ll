; A call to an undefined weak function becomes a nop, its address is 0, and
; relocations in DWARF sections are left alone.
; RUN: rm -rf %t && mkdir -p %t
; RUN: llc -mtriple=powerpc-ibm-aix -filetype=obj %S/Inputs/start.ll -o %t/start.o
; RUN: llc -mtriple=powerpc-ibm-aix -filetype=obj %s -o %t/a.o
; RUN: llvm-readobj --sections %t/a.o | FileCheck %s --check-prefix=OBJ
; RUN: xcoff-ld -o %t/a %t/start.o %t/a.o -bI:%S/Inputs/unix.exp
; RUN: llvm-objdump -d --no-show-raw-insn %t/a | FileCheck %s

; OBJ: Name: .dwinfo
; OBJ: NumberOfRelocations: {{[1-9]}}

; CHECK:       cmplwi 3, 0
; CHECK:       li 3, 1
; CHECK-NEXT:  stw 0, 72(1)
; CHECK-NEXT:  nop
; CHECK-NEXT:  nop
; CHECK-NEXT:  addi 1, 1, 64

target datalayout = "E-m:a-p:32:32-Fi32-i64:64-n32-f64:32:64"
target triple = "powerpc-ibm-aix"

declare extern_weak i32 @hook(i32)

define i32 @main() !dbg !4 {
entry:
  %t = icmp eq ptr @hook, null, !dbg !7
  br i1 %t, label %done, label %call, !dbg !7

call:
  %r = call i32 @hook(i32 1), !dbg !7
  br label %done, !dbg !7

done:
  %v = phi i32 [ %r, %call ], [ 3, %entry ], !dbg !7
  ret i32 %v, !dbg !7
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}
!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, emissionKind: FullDebug)
!1 = !DIFile(filename: "weak.c", directory: "/")
!2 = !{i32 7, !"Dwarf Version", i32 3}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1, type: !5, scopeLine: 1, spFlags: DISPFlagDefinition, unit: !0)
!5 = !DISubroutineType(types: !6)
!6 = !{null}
!7 = !DILocation(line: 1, scope: !4)
