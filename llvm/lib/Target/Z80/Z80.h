//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80_H
#define LLVM_LIB_TARGET_Z80_Z80_H

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "llvm/Support/CodeGen.h"

namespace llvm {
class FunctionPass;
class PassRegistry;
class Z80TargetMachine;

FunctionPass *createZ80ISelDag(Z80TargetMachine &TM, CodeGenOptLevel OptLevel);
FunctionPass *createZ80ExpandPseudoPass();
FunctionPass *createZ80SplitNarrowPass();

void initializeZ80AsmPrinterPass(PassRegistry &);
void initializeZ80DAGToDAGISelLegacyPass(PassRegistry &);
void initializeZ80ExpandPseudoPass(PassRegistry &);
void initializeZ80SplitNarrowPass(PassRegistry &);
} // namespace llvm

#endif
