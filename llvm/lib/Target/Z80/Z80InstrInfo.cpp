//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80InstrInfo.h"
#include "Z80.h"
#include "Z80Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "Z80GenInstrInfo.inc"

Z80InstrInfo::Z80InstrInfo(const Z80Subtarget &STI)
    : Z80GenInstrInfo(STI, RI, Z80::ADJCALLSTACKDOWN, Z80::ADJCALLSTACKUP),
      RI() {}

void Z80InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator I,
                               const DebugLoc &DL, Register DestReg,
                               Register SrcReg, bool KillSrc,
                               bool RenamableDest, bool RenamableSrc) const {
  if (Z80::GR8RegClass.contains(DestReg, SrcReg)) {
    BuildMI(MBB, I, DL, get(Z80::LDrr), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  if (Z80::GR16RegClass.contains(DestReg, SrcReg)) {
    BuildMI(MBB, I, DL, get(Z80::LDrr), RI.getSubReg(DestReg, Z80::sub_lo))
        .addReg(RI.getSubReg(SrcReg, Z80::sub_lo), getKillRegState(KillSrc));
    BuildMI(MBB, I, DL, get(Z80::LDrr), RI.getSubReg(DestReg, Z80::sub_hi))
        .addReg(RI.getSubReg(SrcReg, Z80::sub_hi), getKillRegState(KillSrc));
    return;
  }
  if (DestReg == Z80::SP && SrcReg == Z80::HL) {
    BuildMI(MBB, I, DL, get(Z80::LDSPHL), Z80::SP)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  if (DestReg == Z80::HL && SrcReg == Z80::SP) {
    // This changes the carry flag. Only stacksave copies SP.
    BuildMI(MBB, I, DL, get(Z80::LD16ri), Z80::HL).addImm(0);
    BuildMI(MBB, I, DL, get(Z80::ADDHLSP), Z80::HL)
        .addReg(Z80::HL)
        .addReg(Z80::SP);
    return;
  }
  report_fatal_error("Z80: impossible register copy");
}

static MachineMemOperand *getFrameMMO(MachineBasicBlock &MBB, int FI,
                                      MachineMemOperand::Flags Flags) {
  MachineFunction &MF = *MBB.getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MF.getMachineMemOperand(MachinePointerInfo::getFixedStack(MF, FI),
                                 Flags, MFI.getObjectSize(FI),
                                 MFI.getObjectAlign(FI));
}

void Z80InstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    MachineInstr::MIFlag Flags) const {
  unsigned Opc;
  if (Z80::GR8RegClass.hasSubClassEq(RC))
    Opc = Z80::ST8fi;
  else if (Z80::GR16RegClass.hasSubClassEq(RC))
    Opc = Z80::ST16fi;
  else
    llvm_unreachable("Z80: cannot spill this register class");
  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
  BuildMI(MBB, MI, DL, get(Opc))
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addReg(SrcReg, getKillRegState(isKill))
      .addMemOperand(getFrameMMO(MBB, FrameIndex, MachineMemOperand::MOStore))
      .setMIFlag(Flags);
}

void Z80InstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
    int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    unsigned SubReg, MachineInstr::MIFlag Flags) const {
  unsigned Opc;
  if (Z80::GR8RegClass.hasSubClassEq(RC))
    Opc = Z80::LD8fi;
  else if (Z80::GR16RegClass.hasSubClassEq(RC))
    Opc = Z80::LD16fi;
  else
    llvm_unreachable("Z80: cannot reload this register class");
  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
  BuildMI(MBB, MI, DL, get(Opc), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addMemOperand(getFrameMMO(MBB, FrameIndex, MachineMemOperand::MOLoad))
      .setMIFlag(Flags);
}

unsigned Z80InstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  if (unsigned Size = MI.getDesc().getSize())
    return Size;
  switch (MI.getOpcode()) {
  case TargetOpcode::INLINEASM:
  case TargetOpcode::INLINEASM_BR: {
    const MachineFunction *MF = MI.getParent()->getParent();
    return getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                              MF->getTarget().getMCAsmInfo());
  }
  default:
    // A stack access may take up to about 16 bytes, the other pseudos less.
    return MI.isPseudo() && !MI.isMetaInstruction() ? 16 : 0;
  }
}

bool Z80InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                 MachineBasicBlock *&TBB,
                                 MachineBasicBlock *&FBB,
                                 SmallVectorImpl<MachineOperand> &Cond,
                                 bool AllowModify) const {
  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (!isUnpredicatedTerminator(*I))
      break;
    if (!I->isBranch())
      return true;

    if (I->getOpcode() == Z80::JP) {
      if (!AllowModify) {
        TBB = I->getOperand(0).getMBB();
        continue;
      }
      // Anything after an unconditional branch is dead.
      MBB.erase(std::next(I), MBB.end());
      Cond.clear();
      FBB = nullptr;
      if (MBB.isLayoutSuccessor(I->getOperand(0).getMBB())) {
        TBB = nullptr;
        I->eraseFromParent();
        I = MBB.end();
        continue;
      }
      TBB = I->getOperand(0).getMBB();
      continue;
    }

    if (I->getOpcode() != Z80::JPcc)
      return true;

    int64_t CC = I->getOperand(0).getImm();
    if (Cond.empty()) {
      FBB = TBB;
      TBB = I->getOperand(1).getMBB();
      Cond.push_back(MachineOperand::CreateImm(CC));
      continue;
    }
    // Two conditional branches.
    return true;
  }
  return false;
}

unsigned Z80InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                    int *BytesRemoved) const {
  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (I->getOpcode() != Z80::JP && I->getOpcode() != Z80::JPcc)
      break;
    if (BytesRemoved)
      *BytesRemoved += getInstSizeInBytes(*I);
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }
  return Count;
}

unsigned Z80InstrInfo::insertBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *TBB,
                                    MachineBasicBlock *FBB,
                                    ArrayRef<MachineOperand> Cond,
                                    const DebugLoc &DL, int *BytesAdded) const {
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert(Cond.size() <= 1 && "Z80 branch conditions have one operand");
  if (BytesAdded)
    *BytesAdded = 0;
  if (Cond.empty()) {
    BuildMI(&MBB, DL, get(Z80::JP)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += 3;
    return 1;
  }
  BuildMI(&MBB, DL, get(Z80::JPcc)).addImm(Cond[0].getImm()).addMBB(TBB);
  unsigned Count = 1;
  if (BytesAdded)
    *BytesAdded += 3;
  if (FBB) {
    BuildMI(&MBB, DL, get(Z80::JP)).addMBB(FBB);
    ++Count;
    if (BytesAdded)
      *BytesAdded += 3;
  }
  return Count;
}

bool Z80InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "invalid branch condition");
  Cond[0].setImm(
      Z80::getOppositeCondition(Z80::CondCode(Cond[0].getImm())));
  return false;
}
