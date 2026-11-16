//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80RegisterInfo.h"
#include "Z80.h"
#include "Z80FrameLowering.h"
#include "Z80Subtarget.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "z80-reg-info"

#define GET_REGINFO_TARGET_DESC
#include "Z80GenRegisterInfo.inc"

Z80RegisterInfo::Z80RegisterInfo() : Z80GenRegisterInfo(Z80::PC) {}

const MCPhysReg *
Z80RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_Z80_SaveList;
}

const uint32_t *Z80RegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                                      CallingConv::ID) const {
  return CSR_Z80_RegMask;
}

BitVector Z80RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  for (MCPhysReg R : {Z80::SP, Z80::PC, Z80::F, Z80::AF, Z80::IX, Z80::IY})
    Reserved.set(R);
  return Reserved;
}

// A stack object becomes SP plus its offset from SP after the prologue.
// Z80ExpandPseudo adds what the PUSHes of a call sequence move SP by.
bool Z80RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                          int SPAdj, unsigned FIOperandNum,
                                          RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  int FI = MI.getOperand(FIOperandNum).getIndex();
  int64_t Offset = MFI.getObjectOffset(FI) + MFI.getStackSize() +
                   MI.getOperand(FIOperandNum + 1).getImm();
  MI.getOperand(FIOperandNum).ChangeToRegister(Z80::SP, false);
  MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
  return false;
}

// Many instructions want their operand in HL or A. Joining a value with a
// copy into or out of one of those would tie it to that one register for its
// whole life, where it gets in the way of the next value that needs it.
bool Z80RegisterInfo::shouldCoalesce(MachineInstr *MI,
                                     const TargetRegisterClass *SrcRC,
                                     unsigned SubReg,
                                     const TargetRegisterClass *DstRC,
                                     unsigned DstSubReg,
                                     const TargetRegisterClass *NewRC,
                                     LiveIntervals &LIS) const {
  if (NewRC->getNumRegs() == 1 && (SrcRC != NewRC || DstRC != NewRC))
    return false;
  return true;
}

Register Z80RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return Z80::SP;
}
