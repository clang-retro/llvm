; The SjLj function context stays in the stack frame: the dispatch code that
; instruction selection makes addresses it through its frame index. Other
; allocas whose address escapes still move to the unsafe stack.
; RUN: opt -safe-stack -S -mtriple=i386-pc-linux-gnu < %s -o - | FileCheck %s
; RUN: opt -passes='require<libcall-lowering-info>,safe-stack' -S -mtriple=i386-pc-linux-gnu < %s -o - | FileCheck %s

%fc = type { ptr, i32, [4 x i32], ptr, ptr, [5 x ptr] }

declare void @llvm.eh.sjlj.functioncontext(ptr)
declare void @_Unwind_SjLj_Register(ptr)
declare void @use(ptr)

; CHECK-LABEL: define void @f(
define void @f() safestack {
entry:
  ; CHECK: %unsafe_stack_ptr = load ptr, ptr @__safestack_unsafe_stack_ptr
  ; CHECK: %ctx = alloca %fc
  ; CHECK-NOT: %x = alloca
  %ctx = alloca %fc, align 4
  %x = alloca i32, align 4
  call void @llvm.eh.sjlj.functioncontext(ptr %ctx)
  ; CHECK: call void @_Unwind_SjLj_Register(ptr %ctx)
  call void @_Unwind_SjLj_Register(ptr %ctx)
  call void @use(ptr %x)
  ret void
}
