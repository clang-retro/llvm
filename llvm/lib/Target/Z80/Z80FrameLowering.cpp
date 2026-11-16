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
// SP += Amount, keeping HL and DE (arguments on entry, results on return).
// A large adjustment goes through HL, keeping HL in BC meanwhile, so BC
// must be free: saved by the function, or about to be restored.
static void adjustSP(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                     const DebugLoc &DL, const TargetInstrInfo &TII,
                     int64_t Amount, MachineInstr::MIFlag Flag, bool BCFree) {
  if (Amount == 0)
    return;
  uint64_t Mag = Amount < 0 ? -Amount : Amount;
  if (Mag <= 8 || !BCFree) {
    // PUSH BC only stores BC; POP AF changes nothing that a caller sees.
    for (uint64_t I = 0; I < Mag / 2; ++I) {
      if (Amount < 0)
        BuildMI(MBB, MBBI, DL, TII.get(Z80::PUSHrp))
            .addReg(Z80::BC, RegState::Undef)
            .setMIFlag(Flag);
      else
        BuildMI(MBB, MBBI, DL, TII.get(Z80::POPAF), Z80::AF).setMIFlag(Flag);
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

static bool savesBC(const MachineFunction &MF) {
  for (const CalleeSavedInfo &I : MF.getFrameInfo().getCalleeSavedInfo())
    if (I.getReg() == Z80::BC)
      return true;
  return false;
}

// The bytes of the frame that the callee-saved register pushes take.
static int64_t calleeSavedSize(const MachineFunction &MF) {
  return 2 * int64_t(MF.getFrameInfo().getCalleeSavedInfo().size());
}

int Z80FrameLowering::ixFrameBias(const MachineFunction &MF) const {
  if (!STI.hasZ80())
    return -1;
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  int64_t End = MFI.getStackSize();
  // The callee-saved register pushes are not reached through IX.
  bool Any = End - calleeSavedSize(MF) != 0;
  for (int FI = MFI.getObjectIndexBegin(); FI != 0; ++FI) {
    if (MFI.isDeadObjectIndex(FI) ||
        llvm::any_of(MFI.getCalleeSavedInfo(),
                     [&](const CalleeSavedInfo &I) {
                       return I.getFrameIdx() == FI;
                     }))
      continue;
    Any = true;
    End = std::max(End, int64_t(MFI.getStackSize()) + MFI.getObjectOffset(FI) +
                            int64_t(MFI.getObjectSize(FI)));
  }
  // (IX+d) reaches d in -128..127, so IX can point up to 128 bytes into the
  // frame, and frames of up to 256 bytes (including the incoming arguments)
  // fit.
  if (!Any || End > 256)
    return -1;
  return End > 128 ? int(End - 128) : 0;
}

bool Z80FrameLowering::usesIXFrame(const MachineFunction &MF) const {
  return ixFrameBias(MF) >= 0;
}

// The frame is the locals, at SP after the prologue, then the return
// address and the stack arguments. With an IX frame, the caller's IX is
// pushed below it and IX points at the locals.
void Z80FrameLowering::determineCalleeSaves(MachineFunction &MF,
                                            BitVector &SavedRegs,
                                            RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
  // A frame of more than 8 bytes is allocated with LD HL,-n; ADD HL,SP;
  // LD SP,HL, keeping HL in BC meanwhile.
  if (MF.getFrameInfo().estimateStackSize(MF) > 8)
    SavedRegs.set(Z80::BC);
}

bool Z80FrameLowering::assignCalleeSavedSpillSlots(
    MachineFunction &MF, const TargetRegisterInfo *TRI,
    std::vector<CalleeSavedInfo> &CSI) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  int64_t Off = 0;
  for (CalleeSavedInfo &I : CSI) {
    Off -= 2;
    I.setFrameIdx(MFI.CreateFixedSpillStackObject(2, Off));
  }
  return true;
}

bool Z80FrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
  for (const CalleeSavedInfo &I : CSI) {
    MBB.addLiveIn(I.getReg());
    BuildMI(MBB, MI, DL, TII.get(Z80::PUSHrp))
        .addReg(I.getReg(), RegState::Kill)
        .setMIFlag(MachineInstr::FrameSetup);
  }
  return true;
}

bool Z80FrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI,
    const TargetRegisterInfo *TRI) const {
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
  for (const CalleeSavedInfo &I : llvm::reverse(CSI))
    BuildMI(MBB, MI, DL, TII.get(Z80::POPrp), I.getReg())
        .setMIFlag(MachineInstr::FrameDestroy);
  return true;
}

void Z80FrameLowering::emitPrologue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator MBBI = MBB.begin();
  // After the callee-saved register pushes.
  while (MBBI != MBB.end() && MBBI->getOpcode() == Z80::PUSHrp &&
         MBBI->getFlag(MachineInstr::FrameSetup))
    ++MBBI;
  DebugLoc DL;
  adjustSP(MBB, MBBI, DL, TII,
           -(int64_t(MFI.getStackSize()) - calleeSavedSize(MF)),
           MachineInstr::FrameSetup, savesBC(MF));
  int Bias = ixFrameBias(MF);
  if (Bias >= 0) {
    BuildMI(MBB, MBBI, DL, TII.get(Z80::IXPUSH))
        .addReg(Z80::IX)
        .setMIFlag(MachineInstr::FrameSetup);
    BuildMI(MBB, MBBI, DL, TII.get(Z80::IXLDri), Z80::IX)
        .addImm(2 + Bias)
        .setMIFlag(MachineInstr::FrameSetup);
    BuildMI(MBB, MBBI, DL, TII.get(Z80::IXADDSP), Z80::IX)
        .addReg(Z80::IX)
        .addReg(Z80::SP)
        .setMIFlag(MachineInstr::FrameSetup);
  }
}

void Z80FrameLowering::emitEpilogue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineBasicBlock::iterator MBBI = MBB.getFirstTerminator();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  // Before the callee-saved register pops.
  while (MBBI != MBB.begin()) {
    MachineBasicBlock::iterator P = std::prev(MBBI);
    if (P->getOpcode() != Z80::POPrp ||
        !P->getFlag(MachineInstr::FrameDestroy))
      break;
    MBBI = P;
  }
  if (usesIXFrame(MF))
    BuildMI(MBB, MBBI, DL, STI.getInstrInfo()->get(Z80::IXPOP), Z80::IX)
        .setMIFlag(MachineInstr::FrameDestroy);
  adjustSP(MBB, MBBI, DL, *STI.getInstrInfo(),
           int64_t(MFI.getStackSize()) - calleeSavedSize(MF),
           MachineInstr::FrameDestroy, savesBC(MF));
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
