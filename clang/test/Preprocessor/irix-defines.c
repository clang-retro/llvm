// RUN: %clang -E -dM --target=mips-sgi-irix6.5 %s -o - \
// RUN:   | FileCheck %s --check-prefixes=CHECK,O32
// RUN: %clang -E -dM --target=mips64-sgi-irix6.5 -mabi=n32 %s -o - \
// RUN:   | FileCheck %s --check-prefixes=CHECK,N32
// RUN: %clang -E -dM --target=mips64-sgi-irix6.5 -mabi=64 -march=mips4 %s -o - \
// RUN:   | FileCheck %s --check-prefixes=CHECK,N64
// RUN: %clang -E -dM --target=mips-sgi-irix6.5 -std=c99 %s -o - \
// RUN:   | FileCheck %s --check-prefix=STRICT

// IRIX predefines what SGI's compilers and gcc for IRIX did, which the IRIX
// headers test for.

// CHECK-DAG: #define __sgi 1
// CHECK-DAG: #define sgi 1
// CHECK-DAG: #define __unix 1
// CHECK-DAG: #define unix 1
// CHECK-DAG: #define __host_mips 1
// CHECK-DAG: #define _SYSTYPE_SVR4 1
// CHECK-DAG: #define _SVR4_SOURCE 1
// CHECK-DAG: #define _MODERN_C 1
// CHECK-DAG: #define _LONGLONG 1
// CHECK-DAG: #define _LANGUAGE_C 1
// CHECK-DAG: #define _SGI_SOURCE 1
// CHECK-DAG: #define _MIPSEB 1
// CHECK-DAG: #define __ELF__ 1

// O32-DAG: #define _MIPS_SIM _ABIO32
// O32-DAG: #define _MIPS_ISA _MIPS_ISA_MIPS2
// O32-DAG: #define __mips 2
// O32-DAG: #define _MIPS_SZLONG 32
// O32-DAG: #define __WCHAR_TYPE__ long int
// O32-DAG: #define __SIZEOF_LONG_DOUBLE__ 8

// N32-DAG: #define _MIPS_SIM _ABIN32
// N32-DAG: #define _MIPS_ISA _MIPS_ISA_MIPS3
// N32-DAG: #define __mips 3
// N32-DAG: #define _MIPS_SZLONG 32
// N32-DAG: #define _MIPS_SZPTR 32
// N32-DAG: #define __WCHAR_TYPE__ long int

// N64-DAG: #define _MIPS_SIM _ABI64
// N64-DAG: #define _MIPS_ISA _MIPS_ISA_MIPS4
// N64-DAG: #define __mips 4
// N64-DAG: #define _MIPS_SZLONG 64
// N64-DAG: #define __WCHAR_TYPE__ int

// STRICT-NOT: #define _SGI_SOURCE
// STRICT-NOT: #define sgi 1
// STRICT: #define __sgi 1
