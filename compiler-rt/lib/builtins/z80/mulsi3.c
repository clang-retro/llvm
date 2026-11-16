//===-- mulsi3.c - 32-bit multiplication ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The Z80 and the 8080 multiply 32-bit values with this; 16-bit ones with
// __mulhi3.
//
//===----------------------------------------------------------------------===//

#include "../int_lib.h"

COMPILER_RT_ABI su_int __mulsi3(su_int a, su_int b) {
  su_int r = 0;
  while (b) {
    if (b & 1)
      r += a;
    a <<= 1;
    b >>= 1;
  }
  return r;
}
