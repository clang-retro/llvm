// RUN: %clang -E -dM --target=i386-pc-sco %s -o - \
// RUN:   | FileCheck %s --check-prefixes=CHECK,GNU
// RUN: %clang -E -dM --target=i386-pc-sco -std=c99 %s -o - \
// RUN:   | FileCheck %s --check-prefixes=CHECK,STRICT

// SCO UnixWare and OpenServer predefine what SCO's compilers and gcc for
// them did.

// CHECK-DAG: #define __unix__ 1
// CHECK-DAG: #define __unix 1
// CHECK-DAG: #define __svr4__ 1
// CHECK-DAG: #define __SVR4 1
// CHECK-DAG: #define __SCO__ 1
// CHECK-DAG: #define _M_UNIX 1
// CHECK-DAG: #define _M_I386 1
// CHECK-DAG: #define __ELF__ 1
// CHECK-DAG: #define __i386__ 1
// CHECK-DAG: #define __WCHAR_TYPE__ long int
// GNU-DAG: #define unix 1
// GNU-DAG: #define M_UNIX 1
// GNU-DAG: #define M_I386 1
// STRICT-NOT: #define unix 1
// STRICT-NOT: #define M_UNIX 1
