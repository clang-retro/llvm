target datalayout = "E-m:a-p:32:32-Fi32-i64:64-n32"
target triple = "powerpc-ibm-aix"

declare i32 @main()
declare void @_exit(i32)

define void @__start() {
  %r = call i32 @main()
  call void @_exit(i32 %r)
  unreachable
}
