// RUN: %clang_cc1 -triple z80-unknown-cpm -E -dM < /dev/null | FileCheck %s --check-prefixes=CHECK,Z80,CPM
// RUN: %clang_cc1 -triple i8080-unknown-cpm -E -dM < /dev/null | FileCheck %s --check-prefixes=CHECK,CPM --implicit-check-not=__z80__ --implicit-check-not=__Z80__
// RUN: %clang_cc1 -triple z80-unknown-unknown -E -dM < /dev/null | FileCheck %s --check-prefixes=CHECK,Z80 --implicit-check-not=__CPM__

// CHECK-DAG: #define __i8080__ 1
// CHECK-DAG: #define __8080__ 1
// CHECK-DAG: #define __SIZEOF_INT__ 2
// CHECK-DAG: #define __SIZEOF_LONG__ 4
// CHECK-DAG: #define __SIZEOF_LONG_LONG__ 8
// CHECK-DAG: #define __SIZEOF_POINTER__ 2
// CHECK-DAG: #define __SIZEOF_DOUBLE__ 8
// CHECK-DAG: #define __SIZEOF_LONG_DOUBLE__ 8
// CHECK-DAG: #define __BIGGEST_ALIGNMENT__ 1
// CHECK-DAG: #define __ORDER_LITTLE_ENDIAN__ 1234
// CHECK-DAG: #define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
// Z80-DAG:   #define __z80__ 1
// Z80-DAG:   #define __Z80__ 1
// CPM-DAG:   #define __CPM__ 1
// CPM-DAG:   #define __CPM80__ 1
