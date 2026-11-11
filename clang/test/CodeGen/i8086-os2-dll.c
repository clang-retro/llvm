// In an OS/2 1.x DLL (-mwindows-dll, which -shared gives), exported
// functions load the DLL's data segment themselves, as with __loadds; OS/2
// does not do it for them. Other functions, and Windows DLLs, do not.
// RUN: %clang_cc1 -triple i8086-pc-os2 -target-abi large -mwindows-dll -emit-llvm -o - %s | FileCheck %s --check-prefix=DLL
// RUN: %clang_cc1 -triple i8086-pc-os2 -target-abi large -emit-llvm -o - %s | FileCheck %s --check-prefix=NODLL
// RUN: %clang_cc1 -triple i8086-pc-win16 -target-abi large -mwindows-dll -emit-llvm -o - %s | FileCheck %s --check-prefix=NODLL

__attribute__((dllexport)) int exported(void) { return 0; }
int internal(void) { return 0; }

// DLL: define {{.*}}@exported() {{.*}}#[[E:[0-9]+]]
// DLL: define {{.*}}@internal() {{.*}}#[[I:[0-9]+]]
// DLL: attributes #[[E]] = {{{.*}}"i8086-loadds"
// DLL-NOT: "i8086-loadds"
// NODLL-NOT: "i8086-loadds"
