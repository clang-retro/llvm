target datalayout = "E-m:a-p:32:32-Fi32-i64:64-n32"
target triple = "powerpc-ibm-aix"

define i32 @never_called() {
  ret i32 0
}
