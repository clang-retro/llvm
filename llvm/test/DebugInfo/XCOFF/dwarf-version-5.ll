; XCOFF has sections only for DWARF v2 to v4. When the IR asks for DWARF v5,
; llc warns and emits DWARF v4, in place of a crash on the missing
; .debug_str_offsets section. Check for AIX and for classic Mac OS on PowerPC.

; RUN: llc -mtriple=powerpc-ibm-aix-xcoff -mcpu=ppc -filetype=obj %s -o %t.aix.o \
; RUN:   2>&1 | FileCheck %s --check-prefix=WARN
; RUN: llvm-dwarfdump --debug-info %t.aix.o | FileCheck %s --check-prefix=DWARF
; RUN: llvm-dwarfdump --verify %t.aix.o | FileCheck %s --check-prefix=VERIFY

; RUN: llc -mtriple=powerpc-apple-macos9 -filetype=obj %s -o %t.mac.o \
; RUN:   2>&1 | FileCheck %s --check-prefix=WARN
; RUN: llvm-dwarfdump --debug-info %t.mac.o | FileCheck %s --check-prefix=DWARF
; RUN: llvm-dwarfdump --verify %t.mac.o | FileCheck %s --check-prefix=VERIFY
; RUN: llvm-readobj --sections %t.mac.o | FileCheck %s --check-prefix=SECTIONS

; DWARF version 6 is clamped the same way.
; RUN: sed 's/i32 7, !"Dwarf Version", i32 5/i32 7, !"Dwarf Version", i32 6/' %s \
; RUN:   | llc -mtriple=powerpc-apple-macos9 -filetype=obj -o %t.v6.o 2>&1 \
; RUN:   | FileCheck %s --check-prefix=WARN6
; RUN: llvm-dwarfdump --debug-info %t.v6.o | FileCheck %s --check-prefix=DWARF

; WARN: warning: DWARF version 5 is not supported for XCOFF; emitting DWARF version 4
; WARN6: warning: DWARF version 6 is not supported for XCOFF; emitting DWARF version 4

; DWARF: Compile Unit: {{.*}} version = 0x0004
; DWARF: DW_TAG_compile_unit
; DWARF: DW_AT_name ("t.c")
; DWARF: DW_TAG_subprogram
; DWARF: DW_AT_name ("f")

; VERIFY: No errors.

; SECTIONS-NOT: Name: .dwstroff
; SECTIONS: Name: .dwabrev
; SECTIONS: Name: .dwinfo
; SECTIONS: Name: .dwline

define i32 @f(i32 %x) !dbg !7 {
entry:
  %add = add nsw i32 %x, 1, !dbg !12
  ret i32 %add, !dbg !12
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, nameTableKind: Apple)
!1 = !DIFile(filename: "t.c", directory: "/tmp")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!7 = distinct !DISubprogram(name: "f", scope: !1, file: !1, line: 1, type: !8, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!12 = !DILocation(line: 1, column: 20, scope: !7)
