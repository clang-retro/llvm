//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80SUBTARGET_H
#define LLVM_LIB_TARGET_Z80_Z80SUBTARGET_H

#include "Z80FrameLowering.h"
#include "Z80ISelLowering.h"
#include "Z80InstrInfo.h"
#include "Z80RegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include <memory>
#include <string>

#define GET_SUBTARGETINFO_HEADER
#include "Z80GenSubtargetInfo.inc"

namespace llvm {
class Z80TargetMachine;

class Z80Subtarget : public Z80GenSubtargetInfo {
  bool HasZ80 = false;
  Z80InstrInfo InstrInfo;
  Z80FrameLowering FrameLowering;
  Z80TargetLowering TLInfo;
  std::unique_ptr<const SelectionDAGTargetInfo> TSInfo;

  Z80Subtarget &initializeSubtargetDependencies(const Triple &TT,
                                                StringRef CPU, StringRef FS);

public:
  Z80Subtarget(const Triple &TT, const std::string &CPU, const std::string &FS,
               const Z80TargetMachine &TM);
  ~Z80Subtarget() override;

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  bool hasZ80() const { return HasZ80; }

  const Z80InstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const Z80FrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const Z80TargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const Z80RegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return TSInfo.get();
  }
};

} // namespace llvm

#endif
