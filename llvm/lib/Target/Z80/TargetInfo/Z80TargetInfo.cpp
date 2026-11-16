//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/Z80TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
using namespace llvm;

Target &llvm::getTheZ80Target() {
  static Target TheZ80Target;
  return TheZ80Target;
}

Target &llvm::getTheI8080Target() {
  static Target TheI8080Target;
  return TheI8080Target;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeZ80TargetInfo() {
  RegisterTarget<Triple::z80> X(getTheZ80Target(), "z80", "Zilog Z80", "Z80");
  RegisterTarget<Triple::i8080> Y(getTheI8080Target(), "i8080", "Intel 8080",
                                  "Z80");
}
