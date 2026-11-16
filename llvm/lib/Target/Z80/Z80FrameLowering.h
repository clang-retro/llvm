//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80FRAMELOWERING_H
#define LLVM_LIB_TARGET_Z80_Z80FRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {
class Z80Subtarget;

class Z80FrameLowering : public TargetFrameLowering {
  const Z80Subtarget &STI;

protected:
  bool hasFPImpl(const MachineFunction &MF) const override { return false; }

public:
  explicit Z80FrameLowering(const Z80Subtarget &STI);

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  // Outgoing arguments are pushed.
  // The call frame pseudos are replaced in frame index elimination, which
  // must run even in a function without stack objects.
  bool needsFrameIndexResolution(const MachineFunction &MF) const override {
    return true;
  }

  bool hasReservedCallFrame(const MachineFunction &MF) const override {
    return false;
  }

  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I) const override;
};

} // namespace llvm

#endif
