; RUN: llc -mtriple=m68k -filetype=obj < %s | llvm-dwarfdump --eh-frame - | FileCheck %s

; The call frame information gives the program counter (DWARF register 24,
; as in GCC) as the return address column, a positive CFA offset after the
; stack adjustment, and the place of each callee-saved register in the
; MOVEM that stores them: D2 at the lowest address.

; CHECK:      CIE
; CHECK:      Return address column: 24
; CHECK:      DW_CFA_def_cfa: WSP +4
; CHECK-NEXT: DW_CFA_offset: PC -4
; CHECK:      FDE
; CHECK:      DW_CFA_def_cfa_offset: +24
; CHECK-NEXT: DW_CFA_advance_loc:
; CHECK-NEXT: DW_CFA_offset: WD2 -20
; CHECK-NEXT: DW_CFA_offset: WD3 -16
; CHECK-NEXT: DW_CFA_offset: WD4 -12
; CHECK-NEXT: DW_CFA_offset: WD5 -8

declare i32 @g(i32)

define i32 @f(i32 %a, i32 %b, i32 %c) nounwind uwtable {
entry:
  %x = call i32 @g(i32 %a)
  %y = call i32 @g(i32 %b)
  %z = call i32 @g(i32 %c)
  %s = add i32 %x, %y
  %t = add i32 %s, %z
  %u = add i32 %t, %a
  %v = add i32 %u, %b
  %w = add i32 %v, %c
  ret i32 %w
}
