// The predefined macros of 32-bit DOS (i386-pc-msdos), as Open Watcom
// defines them for its 32-bit DOS targets, and of 16-bit DOS.

// RUN: %clang_cc1 -E -dM -triple i386-pc-msdos %s \
// RUN:   | FileCheck %s --check-prefixes=CHECK,DOS32
// RUN: %clang_cc1 -E -dM -triple i8086-pc-msdos -target-abi small %s \
// RUN:   | FileCheck %s --check-prefixes=CHECK,DOS16
// CHECK-DAG: #define __MSDOS__ 1
// CHECK-DAG: #define __DOS__ 1
// CHECK-DAG: #define _DOS 1
// DOS32-DAG: #define __386__ 1
// DOS32-DAG: #define __FLAT__ 1
// DOS32-DAG: #define __DPMI__ 1
// DOS32-DAG: #define __WCHAR_TYPE__ unsigned short
// DOS32-DAG: #define __SIZEOF_POINTER__ 4
// DOS16-DAG: #define __SMALL__ 1
// DOS16-NOT: #define __386__
