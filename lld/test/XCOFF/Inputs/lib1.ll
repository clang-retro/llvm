target datalayout = "E-m:a-p:32:32-Fi32-i64:64-n32"
target triple = "powerpc-ibm-aix"

@counter = global i32 7

define i32 @helper(i32 %x) {
  %c = load i32, ptr @counter
  %s = add i32 %c, %x
  ret i32 %s
}
