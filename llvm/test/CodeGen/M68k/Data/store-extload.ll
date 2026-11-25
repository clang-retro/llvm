; RUN: llc < %s -mtriple=m68k-apple-macos -O2 | FileCheck %s
;
; A store of a zero- or sign-extended narrower load must not become a
; memory-to-memory move.l: that would copy the bytes after the field
; too.

; CHECK-LABEL: zext16:
; CHECK-NOT: move.l (6,
; CHECK: move.w (6,
define void @zext16(ptr %src, ptr %dst) {
  %p = getelementptr inbounds i8, ptr %src, i32 6
  %v = load i16, ptr %p, align 2
  %z = zext i16 %v to i32
  store i32 %z, ptr %dst, align 2
  ret void
}

; CHECK-LABEL: sext8:
; CHECK-NOT: move.w (3,
; CHECK: move.b (3,
define void @sext8(ptr %src, ptr %dst) {
  %p = getelementptr inbounds i8, ptr %src, i32 3
  %v = load i8, ptr %p, align 1
  %s = sext i8 %v to i16
  store i16 %s, ptr %dst, align 2
  ret void
}

; A plain copy is still one move.
; CHECK-LABEL: copy32:
; CHECK: move.l (4,%a{{[0-9]}}), (%a{{[0-9]}})
define void @copy32(ptr %src, ptr %dst) {
  %p = getelementptr inbounds i8, ptr %src, i32 4
  %v = load i32, ptr %p, align 2
  store i32 %v, ptr %dst, align 2
  ret void
}
