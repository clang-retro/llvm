//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80Subtarget.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80SelectionDAGInfo.h"
#include "Z80TargetMachine.h"

using namespace llvm;

#define DEBUG_TYPE "z80-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "Z80GenSubtargetInfo.inc"

static StringRef defaultCPU(const Triple &TT, StringRef CPU) {
  if (!CPU.empty() && CPU != "generic")
    return CPU;
  return TT.getArch() == Triple::i8080 ? "i8080" : "z80";
}

Z80Subtarget &Z80Subtarget::initializeSubtargetDependencies(const Triple &TT,
                                                            StringRef CPU,
                                                            StringRef FS) {
  StringRef Name = defaultCPU(TT, CPU);
  ParseSubtargetFeatures(Name, /*TuneCPU=*/Name, FS);
  return *this;
}

Z80Subtarget::Z80Subtarget(const Triple &TT, const std::string &CPU,
                           const std::string &FS, const Z80TargetMachine &TM)
    : Z80GenSubtargetInfo(TT, defaultCPU(TT, CPU), defaultCPU(TT, CPU), FS),
      InstrInfo(initializeSubtargetDependencies(TT, CPU, FS)),
      FrameLowering(*this), TLInfo(TM, *this),
      TSInfo(std::make_unique<Z80SelectionDAGInfo>()) {}

Z80Subtarget::~Z80Subtarget() = default;
