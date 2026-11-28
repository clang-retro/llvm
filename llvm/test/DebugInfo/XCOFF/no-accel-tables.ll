; XCOFF has no section for an accelerator table. With LLDB tuning (which
; asks for DWARF v5 .debug_names tables off Mach-O), or with an explicit
; -accel-tables request, llc emits no table, in place of a crash on the
; missing section. Split DWARF (-split-dwarf-file) is ignored for the same
; reason: XCOFF has no .dwo sections.

; RUN: sed 's/TUNE_PLACEHOLDER/lldb/' %s > %t.lldb.ll
; RUN: llc -mtriple=powerpc-apple-macos9 -filetype=obj %t.lldb.ll -o %t.lldb.o
; RUN: llvm-dwarfdump --verify %t.lldb.o | FileCheck %s --check-prefix=VERIFY
; RUN: llc -mtriple=powerpc-ibm-aix-xcoff -mcpu=ppc -filetype=obj %t.lldb.ll \
; RUN:   -o %t.aix.o
; RUN: llvm-dwarfdump --verify %t.aix.o | FileCheck %s --check-prefix=VERIFY
; RUN: llc -mtriple=powerpc-apple-macos9 -filetype=obj -accel-tables=Dwarf \
; RUN:   %s -o %t.dwarf.o
; RUN: llvm-dwarfdump --verify %t.dwarf.o | FileCheck %s --check-prefix=VERIFY
; RUN: llc -mtriple=powerpc-apple-macos9 -filetype=obj -accel-tables=Apple \
; RUN:   %s -o %t.apple.o
; RUN: llvm-dwarfdump --verify %t.apple.o | FileCheck %s --check-prefix=VERIFY
; RUN: llc -mtriple=powerpc-apple-macos9 -filetype=obj \
; RUN:   -split-dwarf-file=t.dwo %s -o %t.split.o
; RUN: llvm-dwarfdump --debug-info %t.split.o | FileCheck %s --check-prefix=FULL

; VERIFY: No errors.

; FULL: DW_TAG_compile_unit
; FULL-NOT: DW_TAG_skeleton_unit
; FULL: DW_TAG_subprogram
; FULL: DW_AT_name ("f")

define i32 @f(i32 %x) !dbg !7 {
entry:
  %add = add nsw i32 %x, 1, !dbg !12
  ret i32 %add, !dbg !12
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, nameTableKind: Apple)
!1 = !DIFile(filename: "t.c", directory: "/tmp")
!2 = !{i32 7, !"Dwarf Version", i32 4}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 2, !"debugger-tuning", !"TUNE_PLACEHOLDER"}
!7 = distinct !DISubprogram(name: "f", scope: !1, file: !1, line: 1, type: !8, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!12 = !DILocation(line: 1, column: 20, scope: !7)
