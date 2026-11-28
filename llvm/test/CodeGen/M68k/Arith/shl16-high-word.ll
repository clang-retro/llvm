; RUN: llc < %s -mtriple=m68k | FileCheck %s

; (shl x, 16) is swap and clr.w. clr.w keeps the high word, so a copy of the
; result to an address register must copy all 32 bits (move.l), not only the
; low word (move.w sign-extends the low word, which is zero after clr.w).

define ptr @shl16_or_to_ptr(i32 %x) {
; CHECK-LABEL: shl16_or_to_ptr:
; CHECK:         swap %d0
; CHECK-NEXT:    clr.w %d0
; CHECK-NOT:     move.w %d{{[0-9]}}, %a
; CHECK:         move.l %d0, %a0
; CHECK-NEXT:    lea (5,%a0), %a0
  %s = shl i32 %x, 16
  %o = or disjoint i32 %s, 5
  %p = inttoptr i32 %o to ptr
  ret ptr %p
}

define ptr @bswap_to_ptr(i32 %x) {
; CHECK-LABEL: bswap_to_ptr:
; CHECK:         rol.w #8, %d0
; CHECK-NEXT:    swap %d0
; CHECK-NEXT:    rol.w #8, %d0
; CHECK-NOT:     move.w %d{{[0-9]}}, %a
; CHECK:         move.l %d0, %a0
  %b = call i32 @llvm.bswap.i32(i32 %x)
  %p = inttoptr i32 %b to ptr
  ret ptr %p
}

declare i32 @llvm.bswap.i32(i32)
