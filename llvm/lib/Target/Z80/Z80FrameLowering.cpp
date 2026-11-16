//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The frame is the locals and spill slots below the return address:
//
//   SP+N+2...  incoming stack arguments
//   SP+N       return address
//   SP...      the N bytes of the frame
//
// There is no frame pointer, so every stack object is addressed from SP,
// which only the PUSHes of call sequences move.
//
//===----------------------------------------------------------------------===//

#include "Z80FrameLowering.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

Z80FrameLowering::Z80FrameLowering(const Z80Subtarget &STI)
    : TargetFrameLowering(TargetFrameLowering::StackGrowsDown, Align(1), 0,
                          Align(1)),
      STI(STI) {}

// Move SP by Amount, using only BC as a scratch register: on entry HL and DE
// hold arguments, and before RET the return value. Small frames take PUSHes
// or POPs of BC, larger ones ADD HL,SP with HL saved in BC.
static void adjustSP(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                     const DebugLoc &DL, const TargetInstrInfo &TII,
                     int64_t Amount, MachineInstr::MIFlag Flag) {
  if (Amount == 0)
    return;
  uint64_t Mag = Amount < 0 ? -Amount : Amount;
  if (Mag <= 8) {
    for (uint64_t I = 0; I < Mag / 2; ++I) {
      if (Amount < 0)
        BuildMI(MBB, MBBI, DL, TII.get(Z80::PUSHrp))
            .addReg(Z80::BC, RegState::Undef)
            .setMIFlag(Flag);
      else
        BuildMI(MBB, MBBI, DL, TII.get(Z80::POPrp), Z80::BC).setMIFlag(Flag);
    }
    if (Mag % 2)
      BuildMI(MBB, MBBI, DL, TII.get(Amount < 0 ? Z80::DECSP : Z80::INCSP))
          .setMIFlag(Flag);
    return;
  }
  BuildMI(MBB, MBBI, DL, TII.get(Z80::LDrr), Z80::B)
      .addReg(Z80::H)
      .setMIFlag(Flag);
  BuildMI(MBB, MBBI, DL, TII.get(Z80::LDrr), Z80::C)
      .addReg(Z80::L)
      .setMIFlag(Flag);
  BuildMI(MBB, MBBI, DL, TII.get(Z80::LD16ri), Z80::HL)
      .addImm(Amount & 0xffff)
      .setMIFlag(Flag);
  BuildMI(MBB, MBBI, DL, TII.get(Z80::ADDHLSP), Z80::HL)
      .addReg(Z80::HL)
      .addReg(Z80::SP)
      .setMIFlag(Flag);
  BuildMI(MBB, MBBI, DL, TII.get(Z80::LDSPHL), Z80::SP)
      .addReg(Z80::HL)
      .setMIFlag(Flag);
  BuildMI(MBB, MBBI, DL, TII.get(Z80::LDrr), Z80::H)
      .addReg(Z80::B)
      .setMIFlag(Flag);
  BuildMI(MBB, MBBI, DL, TII.get(Z80::LDrr), Z80::L)
      .addReg(Z80::C)
      .setMIFlag(Flag);
}

void Z80FrameLowering::emitPrologue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL;
  adjustSP(MBB, MBBI, DL, *STI.getInstrInfo(), -int64_t(MFI.getStackSize()),
           MachineInstr::FrameSetup);
}

void Z80FrameLowering::emitEpilogue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineBasicBlock::iterator MBBI = MBB.getFirstTerminator();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  adjustSP(MBB, MBBI, DL, *STI.getInstrInfo(), MFI.getStackSize(),
           MachineInstr::FrameDestroy);
}

// The PUSHes of the arguments move SP down, so ADJCALLSTACKDOWN does
// nothing, and ADJCALLSTACKUP pops them off again.
MachineBasicBlock::iterator Z80FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  if (I->getOpcode() == Z80::ADJCALLSTACKUP) {
    int64_t Amount = I->getOperand(0).getImm();
    if (Amount)
      BuildMI(MBB, I, I->getDebugLoc(), TII.get(Z80::SPADJ)).addImm(Amount);
  }
  return MBB.erase(I);
}
