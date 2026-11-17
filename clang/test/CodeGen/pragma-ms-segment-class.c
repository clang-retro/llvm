// RUN: %clang_cc1 -emit-llvm -triple i386-pc-win32 -fms-extensions -verify -o - %s | FileCheck %s
// RUN: %clang_cc1 -emit-llvm -triple i386-pc-windows-gnu -fms-extensions -verify -o - %s | FileCheck %s
// expected-no-diagnostics

// The Windows 9x DDK names a segment's class after its name, and its headers
// define macros that #pragma takes as the pragma, which Microsoft's compiler
// expands.

#define VxD_LOCKED_CODE_SEG code_seg("_LTEXT", "LCODE")
#define VxD_PAGEABLE_DATA_SEG data_seg("_PDATA", "PDATA")

#pragma VxD_LOCKED_CODE_SEG
void locked(void) {}
// CHECK-DAG: define {{.*}}void @locked() {{.*}}section "_LTEXT"

#pragma VxD_PAGEABLE_DATA_SEG
int pageable = 1;
// CHECK-DAG: @pageable = {{.*}}global i32 1, section "_PDATA"

#pragma code_seg("_ITEXT", "ICODE")
void init(void) {}
// CHECK-DAG: define {{.*}}void @init() {{.*}}section "_ITEXT"

#pragma code_seg(push, "_PTEXT", "PCODE")
void pageable_code(void) {}
// CHECK-DAG: define {{.*}}void @pageable_code() {{.*}}section "_PTEXT"
#pragma code_seg(pop)
