; RUN: llc -verify-machineinstrs -mtriple=i686-pc-windows-msvc < %s | FileCheck %s
; RUN: llc -verify-machineinstrs -mtriple=i686-pc-windows-msvc -filetype=obj < %s -o %t

; __CxxFrameHandler is the C++ handler of the C runtime of Visual C++ 2003
; and before (msvcrt.dll to msvcr71.dll). It takes the same funclets and
; state numbers as __CxxFrameHandler3, and the first version of FuncInfo
; (magic number 0x19930520), which has no ESTypeList and no EHFlags.
;
; Loosely based on IR for this C++ source code:
;   void f(int p);
;   struct S { ~S(); };
;   int main() {
;     S s;
;     try {
;       f(1);
;     } catch (int e) {
;       f(e);
;     }
;   }

%rtti.TypeDescriptor2 = type { ptr, ptr, [3 x i8] }

$"\01??_R0H@8" = comdat any

@"\01??_7type_info@@6B@" = external constant ptr
@"\01??_R0H@8" = linkonce_odr global %rtti.TypeDescriptor2 { ptr @"\01??_7type_info@@6B@", ptr null, [3 x i8] c".H\00" }, comdat

declare void @f(i32)
declare void @dtor()
declare i32 @__CxxFrameHandler(...)
declare i32 @__CxxFrameHandler3(...)

define i32 @old_handler() personality ptr @__CxxFrameHandler {
entry:
  %e.addr = alloca i32
  invoke void @f(i32 1)
          to label %try.cont unwind label %catch.dispatch

catch.dispatch:
  %cs = catchswitch within none [label %handler] unwind label %cleanup

handler:
  %h = catchpad within %cs [ptr @"\01??_R0H@8", i32 0, ptr %e.addr]
  %e = load i32, ptr %e.addr
  invoke void @f(i32 %e) [ "funclet"(token %h) ]
          to label %catch.done unwind label %cleanup

catch.done:
  catchret from %h to label %try.cont

try.cont:
  call void @dtor()
  ret i32 0

cleanup:
  %c = cleanuppad within none []
  call void @dtor() [ "funclet"(token %c) ]
  cleanupret from %c unwind to caller
}

define i32 @new_handler() personality ptr @__CxxFrameHandler3 {
entry:
  %e.addr = alloca i32
  invoke void @f(i32 1)
          to label %try.cont unwind label %catch.dispatch

catch.dispatch:
  %cs = catchswitch within none [label %handler] unwind to caller

handler:
  %h = catchpad within %cs [ptr @"\01??_R0H@8", i32 0, ptr %e.addr]
  catchret from %h to label %try.cont

try.cont:
  ret i32 0
}

; FuncInfo for __CxxFrameHandler ends after the IP-to-state map.
; CHECK-LABEL: L__ehtable$old_handler:
; CHECK-NEXT: .long 429065504 # MagicNumber
; CHECK-NEXT: .long 3 # MaxState
; CHECK-NEXT: .long $stateUnwindMap$old_handler # UnwindMap
; CHECK-NEXT: .long 1 # NumTryBlocks
; CHECK-NEXT: .long $tryMap$old_handler # TryBlockMap
; CHECK-NEXT: .long 0 # IPMapEntries
; CHECK-NEXT: .long 0 # IPToStateXData
; CHECK-NEXT: $stateUnwindMap$old_handler:

; __CxxFrameHandler3 keeps the 0x19930522 version.
; CHECK-LABEL: L__ehtable$new_handler:
; CHECK-NEXT: .long 429065506 # MagicNumber
; CHECK: .long 0 # IPToStateXData
; CHECK-NEXT: .long 0 # ESTypeList
; CHECK-NEXT: .long 1 # EHFlags

; Each handler thunk jumps to its own handler.
; CHECK-LABEL: ___ehhandler$old_handler:
; CHECK: movl $L__ehtable$old_handler, %eax
; CHECK-NEXT: jmp ___CxxFrameHandler # TAILCALL
; CHECK-LABEL: ___ehhandler$new_handler:
; CHECK: movl $L__ehtable$new_handler, %eax
; CHECK-NEXT: jmp ___CxxFrameHandler3 # TAILCALL
