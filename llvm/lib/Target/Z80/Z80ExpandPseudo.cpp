//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass expands the pseudo instructions that stand for several real
// ones: the accesses to stack objects, the 16-bit operations that go through
// A, and the stack pointer adjustment after a call. It runs after frame
// lowering, when it knows the registers and flags that are live, and how far
// the PUSHes of a call sequence have moved SP.
//
// The 8080 cannot address the stack except through HL (ADD HL,SP), so a
// stack access needs HL. When HL, a scratch register or the carry flag hold
// something live, the expansion saves them on the stack around the access.
//
//===----------------------------------------------------------------------===//

#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/CodeGen/LiveRegUnits.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "z80-expand-pseudo"
#define PASS_NAME "Z80 pseudo instruction expansion"

namespace {

class Z80ExpandPseudo : public MachineFunctionPass {
public:
  static char ID;
  Z80ExpandPseudo() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return PASS_NAME; }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().setNoVRegs();
  }
};

// Builds the expansion of one pseudo before it, keeping track of SP.
class Builder {
  MachineBasicBlock &MBB;
  MachineBasicBlock::iterator I;
  DebugLoc DL;
  const TargetInstrInfo &TII;
  const TargetRegisterInfo &TRI;

public:
  const LiveRegUnits &Live; // The registers live after the pseudo.
  int &Delta;               // How far SP is below its value after the prologue.
  bool HasZ80;
  bool IXFrame; // Stack objects are at IX + their offset.

  Builder(MachineInstr &MI, const TargetInstrInfo &TII,
          const TargetRegisterInfo &TRI, const LiveRegUnits &Live, int &Delta,
          bool HasZ80, bool IXFrame)
      : MBB(*MI.getParent()), I(MI), DL(MI.getDebugLoc()), TII(TII), TRI(TRI),
        Live(Live), Delta(Delta), HasZ80(HasZ80), IXFrame(IXFrame) {}

  // R = (IX + D) and (IX + D) = R.
  void ldFromIX(MCRegister R, int64_t D) {
    build(Z80::IXLDrd, R).addReg(Z80::IX).addImm(D);
  }
  void stToIX(int64_t D, MCRegister R) {
    build(Z80::IXSTdr).addReg(Z80::IX).addImm(D).addReg(R);
  }

  bool isLive(MCRegister R) const { return !Live.available(R); }
  MCRegister lo(MCRegister R) const { return TRI.getSubReg(R, Z80::sub_lo); }
  MCRegister hi(MCRegister R) const { return TRI.getSubReg(R, Z80::sub_hi); }

  MachineInstrBuilder build(unsigned Opc) {
    return BuildMI(MBB, I, DL, TII.get(Opc));
  }
  MachineInstrBuilder build(unsigned Opc, MCRegister Dst) {
    return BuildMI(MBB, I, DL, TII.get(Opc), Dst);
  }

  void ld(MCRegister Dst, MCRegister Src) {
    if (Dst != Src)
      build(Z80::LDrr, Dst).addReg(Src);
  }
  void ldi(MCRegister Dst, int64_t Imm) {
    build(Z80::LDri, Dst).addImm(Imm & 0xff);
  }
  void ld16i(MCRegister Dst, int64_t Imm) {
    build(Z80::LD16ri, Dst).addImm(Imm & 0xffff);
  }
  // Dst = (HL)
  void ldFromHL(MCRegister Dst) {
    build(Z80::LDrHL, Dst).addReg(Z80::HL);
  }
  // (HL) = Src
  void stToHL(MCRegister Src) {
    build(Z80::LDHLr).addReg(Z80::HL).addReg(Src);
  }
  void incHL() { build(Z80::INC16, Z80::HL).addReg(Z80::HL); }
  void decHL() { build(Z80::DEC16, Z80::HL).addReg(Z80::HL); }
  // EX DE,HL swaps a value with what may be nothing.
  void exDEHL() {
    MachineInstr *MI = build(Z80::EXDEHL);
    for (MachineOperand &MO : MI->implicit_operands())
      if (MO.isUse())
        MO.setIsUndef();
  }
  void alu(unsigned Opc, MCRegister Src) {
    build(Opc, Z80::A).addReg(Z80::A).addReg(Src);
  }
  // AND A: clears the carry, whatever A holds.
  void clearCarry() {
    build(Z80::ANDr, Z80::A)
        .addReg(Z80::A, RegState::Undef)
        .addReg(Z80::A, RegState::Undef);
  }
  void accOp(unsigned Opc) { build(Opc, Z80::A).addReg(Z80::A); }

  void push(MCRegister RP) {
    if (RP == Z80::AF)
      build(Z80::PUSHAF).addReg(Z80::AF, RegState::Undef);
    else
      build(Z80::PUSHrp).addReg(RP, RegState::Undef);
    Delta += 2;
  }
  void pop(MCRegister RP) {
    build(RP == Z80::AF ? Z80::POPAF : Z80::POPrp, RP);
    Delta -= 2;
  }

  // HL = SP + Off, where Off is counted from SP after the prologue.
  void addrToHL(int64_t Off) {
    ld16i(Z80::HL, Off + Delta);
    build(Z80::ADDHLSP, Z80::HL).addReg(Z80::HL).addReg(Z80::SP);
  }

  // An 8-bit register other than H, L and Not that is dead after the pseudo,
  // or none.
  MCRegister deadByte(MCRegister Not1, MCRegister Not2 = MCRegister()) const {
    for (MCRegister R : {Z80::A, Z80::E, Z80::D, Z80::C, Z80::B})
      if (R != Not1 && R != Not2 && !isLive(R))
        return R;
    return MCRegister();
  }

  // A register pair, other than Not, that is dead after the pseudo, or none.
  MCRegister deadPair(MCRegister Not1 = MCRegister(),
                      MCRegister Not2 = MCRegister()) const {
    for (MCRegister R : {Z80::DE, Z80::BC, Z80::HL})
      if (R != Not1 && R != Not2 && !isLive(R))
        return R;
    return MCRegister();
  }
};

} // end anonymous namespace

char Z80ExpandPseudo::ID = 0;

INITIALIZE_PASS(Z80ExpandPseudo, DEBUG_TYPE, PASS_NAME, false, false)

FunctionPass *llvm::createZ80ExpandPseudoPass() {
  return new Z80ExpandPseudo();
}

static bool isHalfOfHL(MCRegister R) { return R == Z80::H || R == Z80::L; }

// The carry flag is the only flag that ADD HL,SP changes. A saved AF is
// restored at the end; if the result of the pseudo is in A, it is written into
// the saved copy first. SavedAFOff is the offset of that copy from SP when the
// result is ready.
static void patchSavedA(Builder &B, int SavedAFOff) {
  // HL is free here: the callers restore it after this.
  B.ld16i(Z80::HL, SavedAFOff + 1);
  B.build(Z80::ADDHLSP, Z80::HL).addReg(Z80::HL).addReg(Z80::SP);
  B.stToHL(Z80::A);
}

// Dst = (SP + Off), 8 bits.
static void expandLD8(Builder &B, MCRegister Dst, int64_t Off) {
  if (B.IXFrame)
    return B.ldFromIX(Dst, Off);
  bool SaveF = B.isLive(Z80::F);
  if (SaveF)
    B.push(Z80::AF);
  int AFAt = B.Delta;

  if (!isHalfOfHL(Dst)) {
    bool SaveHL = B.isLive(Z80::H) || B.isLive(Z80::L);
    if (SaveHL)
      B.push(Z80::HL);
    B.addrToHL(Off);
    B.ldFromHL(Dst);
    if (SaveF && Dst == Z80::A)
      patchSavedA(B, B.Delta - AFAt);
    if (SaveHL)
      B.pop(Z80::HL);
  } else {
    MCRegister Other = Dst == Z80::H ? Z80::L : Z80::H;
    MCRegister T = B.deadByte(MCRegister());
    bool PushDE = !T;
    if (PushDE) {
      B.push(Z80::DE);
      T = Z80::E;
    }
    bool SaveHL = B.isLive(Other);
    if (SaveHL)
      B.push(Z80::HL);
    B.addrToHL(Off);
    B.ldFromHL(T);
    if (SaveHL)
      B.pop(Z80::HL);
    B.ld(Dst, T);
    if (PushDE)
      B.pop(Z80::DE);
  }

  if (SaveF)
    B.pop(Z80::AF);
}

// Dst = (SP + Off), 16 bits.
static void expandLD16(Builder &B, MCRegister Dst, int64_t Off) {
  if (B.IXFrame) {
    B.ldFromIX(B.lo(Dst), Off);
    B.ldFromIX(B.hi(Dst), Off + 1);
    return;
  }
  bool SaveF = B.isLive(Z80::F);
  if (SaveF)
    B.push(Z80::AF);

  if (Dst != Z80::HL) {
    bool SaveHL = B.isLive(Z80::HL);
    if (SaveHL)
      B.push(Z80::HL);
    B.addrToHL(Off);
    B.ldFromHL(B.lo(Dst));
    B.incHL();
    B.ldFromHL(B.hi(Dst));
    if (SaveHL)
      B.pop(Z80::HL);
  } else if (!B.isLive(Z80::A)) {
    B.addrToHL(Off);
    B.ldFromHL(Z80::A);
    B.incHL();
    B.ldFromHL(Z80::H);
    B.ld(Z80::L, Z80::A);
  } else {
    MCRegister P = B.deadPair(Z80::HL);
    bool PushP = !P;
    if (PushP) {
      P = Z80::DE;
      B.push(P);
    }
    B.addrToHL(Off);
    B.ldFromHL(B.lo(P));
    B.incHL();
    B.ldFromHL(B.hi(P));
    if (P == Z80::DE) {
      B.exDEHL();
    } else {
      B.ld(Z80::L, B.lo(P));
      B.ld(Z80::H, B.hi(P));
    }
    if (PushP)
      B.pop(P);
  }

  if (SaveF)
    B.pop(Z80::AF);
}

// (SP + Off) = Src, 8 bits.
static void expandST8(Builder &B, int64_t Off, MCRegister Src) {
  if (B.IXFrame)
    return B.stToIX(Off, Src);
  bool SaveF = B.isLive(Z80::F);
  if (SaveF)
    B.push(Z80::AF);

  bool PushDE = false;
  if (isHalfOfHL(Src)) {
    MCRegister T = B.deadByte(MCRegister());
    // A is free even when live if AF is restored at the end.
    if (!T && SaveF)
      T = Z80::A;
    if (!T) {
      PushDE = true;
      B.push(Z80::DE);
      T = Z80::E;
    }
    B.ld(T, Src);
    Src = T;
  }
  bool SaveHL = B.isLive(Z80::HL);
  if (SaveHL)
    B.push(Z80::HL);
  B.addrToHL(Off);
  B.stToHL(Src);
  if (SaveHL)
    B.pop(Z80::HL);
  if (PushDE)
    B.pop(Z80::DE);

  if (SaveF)
    B.pop(Z80::AF);
}

// (SP + Off) = Src, 16 bits.
static void expandST16(Builder &B, int64_t Off, MCRegister Src) {
  if (B.IXFrame) {
    B.stToIX(Off, B.lo(Src));
    B.stToIX(Off + 1, B.hi(Src));
    return;
  }
  bool SaveF = B.isLive(Z80::F);
  if (SaveF)
    B.push(Z80::AF);

  if (Src != Z80::HL) {
    bool SaveHL = B.isLive(Z80::HL);
    if (SaveHL)
      B.push(Z80::HL);
    B.addrToHL(Off);
    B.stToHL(B.lo(Src));
    B.incHL();
    B.stToHL(B.hi(Src));
    if (SaveHL)
      B.pop(Z80::HL);
  } else {
    // Swap the value into DE, store it and swap back.
    bool SaveDE = B.isLive(Z80::DE);
    B.exDEHL();
    if (SaveDE)
      B.push(Z80::HL);
    B.addrToHL(Off);
    B.stToHL(Z80::E);
    B.incHL();
    B.stToHL(Z80::D);
    if (SaveDE)
      B.pop(Z80::HL);
    B.exDEHL();
  }

  if (SaveF)
    B.pop(Z80::AF);
}

// Dst = SP + Off.
static void expandLEA(Builder &B, MCRegister Dst, int64_t Off) {
  bool SaveF = B.isLive(Z80::F);
  if (SaveF)
    B.push(Z80::AF);
  if (Dst == Z80::HL) {
    B.addrToHL(Off);
  } else {
    bool SaveHL = B.isLive(Z80::HL);
    if (SaveHL)
      B.push(Z80::HL);
    B.addrToHL(Off);
    B.ld(B.lo(Dst), Z80::L);
    B.ld(B.hi(Dst), Z80::H);
    if (SaveHL)
      B.pop(Z80::HL);
  }
  if (SaveF)
    B.pop(Z80::AF);
}

// SP += Amt, after a call.
static void expandSPADJ(Builder &B, int64_t Amt) {
  bool FDead = !B.isLive(Z80::F);
  MCRegister P = B.deadPair();
  if (!P && !B.isLive(Z80::A) && FDead)
    P = Z80::AF;
  if (P && Amt <= 10) {
    for (int64_t N = 0; N < Amt / 2; ++N)
      B.pop(P);
    if (Amt % 2) {
      B.build(Z80::INCSP);
      --B.Delta;
    }
    return;
  }
  MCRegister Save = B.deadPair(Z80::HL);
  if (FDead && (!B.isLive(Z80::HL) || Save)) {
    bool SaveHL = B.isLive(Z80::HL);
    if (SaveHL) {
      B.ld(B.lo(Save), Z80::L);
      B.ld(B.hi(Save), Z80::H);
    }
    B.ld16i(Z80::HL, Amt);
    B.build(Z80::ADDHLSP, Z80::HL).addReg(Z80::HL).addReg(Z80::SP);
    B.build(Z80::LDSPHL, Z80::SP).addReg(Z80::HL);
    if (SaveHL) {
      B.ld(Z80::L, B.lo(Save));
      B.ld(Z80::H, B.hi(Save));
    }
    B.Delta -= Amt;
    return;
  }
  // Everything is live: INC SP changes nothing else.
  for (int64_t N = 0; N < Amt; ++N)
    B.build(Z80::INCSP);
  B.Delta -= Amt;
}

// A 16-bit operation a byte at a time through A: Dst = Dst op Src.
static void expandALU16(Builder &B, unsigned LoOpc, unsigned HiOpc,
                        MCRegister Dst, MCRegister Src, bool Store) {
  B.ld(Z80::A, B.lo(Dst));
  B.alu(LoOpc, B.lo(Src));
  if (Store)
    B.ld(B.lo(Dst), Z80::A);
  B.ld(Z80::A, B.hi(Dst));
  B.alu(HiOpc, B.hi(Src));
  if (Store)
    B.ld(B.hi(Dst), Z80::A);
}

// Dst op= Imm, a byte at a time through A. Logic ops skip bytes they leave
// unchanged, since nothing reads their flags.
static void expandALU16ri(Builder &B, unsigned LoOpc, unsigned HiOpc,
                          MCRegister Dst, uint64_t Imm, bool Store,
                          bool Logic) {
  for (int I = 0; I < 2; ++I) {
    MCRegister R = I ? B.hi(Dst) : B.lo(Dst);
    unsigned Opc = I ? HiOpc : LoOpc;
    unsigned N = (Imm >> (8 * I)) & 0xff;
    if (Logic && Store) {
      if ((Opc == Z80::ANDn && N == 0xff) ||
          ((Opc == Z80::ORn || Opc == Z80::XORn) && N == 0))
        continue;
      if ((Opc == Z80::ANDn && N == 0) || (Opc == Z80::ORn && N == 0xff)) {
        B.ldi(R, N);
        continue;
      }
    }
    B.ld(Z80::A, R);
    if (Logic && Store && Opc == Z80::XORn && N == 0xff)
      B.accOp(Z80::CPL);
    else
      B.build(Opc, Z80::A).addReg(Z80::A).addImm(N);
    if (Store)
      B.ld(R, Z80::A);
  }
}

static bool expandMI(Builder &B, MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case Z80::LD8fi:
    expandLD8(B, MI.getOperand(0).getReg(), MI.getOperand(2).getImm());
    return true;
  case Z80::LD16fi:
    expandLD16(B, MI.getOperand(0).getReg(), MI.getOperand(2).getImm());
    return true;
  case Z80::ST8fi:
    expandST8(B, MI.getOperand(1).getImm(), MI.getOperand(2).getReg());
    return true;
  case Z80::ST16fi:
    expandST16(B, MI.getOperand(1).getImm(), MI.getOperand(2).getReg());
    return true;
  case Z80::LEAfi:
    expandLEA(B, MI.getOperand(0).getReg(), MI.getOperand(2).getImm());
    return true;
  case Z80::SPADJ:
    expandSPADJ(B, MI.getOperand(0).getImm());
    return true;

  case Z80::LD16HL: {
    MCRegister Dst = MI.getOperand(0).getReg();
    MCRegister Ptr = MI.getOperand(1).getReg();
    bool Kill = MI.getOperand(1).isKill() || Dst == Ptr;
    if (Ptr == Z80::HL && Dst != Z80::HL) {
      B.ldFromHL(B.lo(Dst));
      B.incHL();
      B.ldFromHL(B.hi(Dst));
      if (!Kill)
        B.decHL();
      return true;
    }
    // The rest go through A.
    bool SaveA = B.isLive(Z80::A);
    if (SaveA)
      B.push(Z80::AF);
    if (Ptr == Z80::HL) {
      // HL = (HL)
      B.ldFromHL(Z80::A);
      B.incHL();
      B.ldFromHL(Z80::H);
      B.ld(Z80::L, Z80::A);
    } else if (Dst != Ptr) {
      B.build(Z80::LDArp, Z80::A).addReg(Ptr);
      B.ld(B.lo(Dst), Z80::A);
      B.build(Z80::INC16, Ptr).addReg(Ptr);
      B.build(Z80::LDArp, Z80::A).addReg(Ptr);
      B.ld(B.hi(Dst), Z80::A);
      if (!Kill)
        B.build(Z80::DEC16, Ptr).addReg(Ptr);
    } else {
      // rp = (rp): the low byte waits on the stack.
      B.build(Z80::LDArp, Z80::A).addReg(Ptr);
      B.build(Z80::INC16, Ptr).addReg(Ptr);
      B.push(Z80::AF);
      B.build(Z80::LDArp, Z80::A).addReg(Ptr);
      B.ld(B.hi(Dst), Z80::A);
      B.pop(Z80::AF);
      B.ld(B.lo(Dst), Z80::A);
    }
    if (SaveA)
      B.pop(Z80::AF);
    return true;
  }
  case Z80::ST16HL: {
    MCRegister Ptr = MI.getOperand(0).getReg();
    MCRegister Src = MI.getOperand(1).getReg();
    bool Kill = MI.getOperand(0).isKill();
    if (Ptr == Z80::HL && Src != Z80::HL) {
      B.stToHL(B.lo(Src));
      B.incHL();
      B.stToHL(B.hi(Src));
      if (!Kill)
        B.decHL();
      return true;
    }
    // The high byte is read before the pointer moves.
    bool SaveA = B.isLive(Z80::A);
    if (SaveA)
      B.push(Z80::AF);
    if (Ptr == Z80::HL) {
      B.ld(Z80::A, Z80::H);
      B.stToHL(Z80::L);
      B.incHL();
      B.stToHL(Z80::A);
      if (!Kill)
        B.decHL();
    } else {
      B.ld(Z80::A, B.lo(Src));
      B.build(Z80::LDrpA).addReg(Ptr).addReg(Z80::A);
      B.ld(Z80::A, B.hi(Src));
      B.build(Z80::INC16, Ptr).addReg(Ptr);
      B.build(Z80::LDrpA).addReg(Ptr).addReg(Z80::A);
      if (!Kill)
        B.build(Z80::DEC16, Ptr).addReg(Ptr);
    }
    if (SaveA)
      B.pop(Z80::AF);
    return true;
  }

  case Z80::SUB16: {
    MCRegister Dst = MI.getOperand(0).getReg();
    MCRegister Src = MI.getOperand(2).getReg();
    if (B.HasZ80 && Dst == Z80::HL && Src != Z80::HL) {
      B.clearCarry();
      B.build(Z80::SBCHLrp, Z80::HL).addReg(Z80::HL).addReg(Src);
      return true;
    }
    expandALU16(B, Z80::SUBr, Z80::SBCr, Dst, Src, true);
    return true;
  }
  case Z80::SBC16:
    expandALU16(B, Z80::SBCr, Z80::SBCr, MI.getOperand(0).getReg(),
                MI.getOperand(2).getReg(), true);
    return true;
  case Z80::ADC16:
    if (B.HasZ80 && MI.getOperand(0).getReg() == Z80::HL) {
      B.build(Z80::ADCHLrp, Z80::HL)
          .addReg(Z80::HL)
          .addReg(MI.getOperand(2).getReg());
      return true;
    }
    expandALU16(B, Z80::ADCr, Z80::ADCr, MI.getOperand(0).getReg(),
                MI.getOperand(2).getReg(), true);
    return true;
  case Z80::AND16:
    expandALU16(B, Z80::ANDr, Z80::ANDr, MI.getOperand(0).getReg(),
                MI.getOperand(2).getReg(), true);
    return true;
  case Z80::OR16:
    expandALU16(B, Z80::ORr, Z80::ORr, MI.getOperand(0).getReg(),
                MI.getOperand(2).getReg(), true);
    return true;
  case Z80::XOR16:
    expandALU16(B, Z80::XORr, Z80::XORr, MI.getOperand(0).getReg(),
                MI.getOperand(2).getReg(), true);
    return true;
  case Z80::CMP16:
    expandALU16(B, Z80::SUBr, Z80::SBCr, MI.getOperand(0).getReg(),
                MI.getOperand(1).getReg(), false);
    return true;
  case Z80::AND16ri:
  case Z80::OR16ri:
  case Z80::XOR16ri: {
    unsigned Opc = MI.getOpcode() == Z80::AND16ri  ? Z80::ANDn
                   : MI.getOpcode() == Z80::OR16ri ? Z80::ORn
                                                   : Z80::XORn;
    expandALU16ri(B, Opc, Opc, MI.getOperand(0).getReg(),
                  MI.getOperand(2).getImm(), true, true);
    return true;
  }
  case Z80::ADD16ri:
    expandALU16ri(B, Z80::ADDn, Z80::ADCn, MI.getOperand(0).getReg(),
                  MI.getOperand(2).getImm(), true, false);
    return true;
  case Z80::ADC16ri:
    expandALU16ri(B, Z80::ADCn, Z80::ADCn, MI.getOperand(0).getReg(),
                  MI.getOperand(2).getImm(), true, false);
    return true;
  case Z80::SUB16ri:
    expandALU16ri(B, Z80::SUBn, Z80::SBCn, MI.getOperand(0).getReg(),
                  MI.getOperand(2).getImm(), true, false);
    return true;
  case Z80::SBC16ri:
    expandALU16ri(B, Z80::SBCn, Z80::SBCn, MI.getOperand(0).getReg(),
                  MI.getOperand(2).getImm(), true, false);
    return true;
  case Z80::CMP16ri:
    expandALU16ri(B, Z80::SUBn, Z80::SBCn, MI.getOperand(0).getReg(),
                  MI.getOperand(1).getImm(), false, false);
    return true;
  case Z80::TST16: {
    MCRegister R = MI.getOperand(0).getReg();
    B.ld(Z80::A, B.hi(R));
    B.alu(Z80::ORr, B.lo(R));
    return true;
  }

  case Z80::SRL16:
  case Z80::SRA16: {
    MCRegister R = MI.getOperand(0).getReg();
    bool Arith = MI.getOpcode() == Z80::SRA16;
    if (B.HasZ80) {
      B.build(Arith ? Z80::SRAr : Z80::SRLr, B.hi(R)).addReg(B.hi(R));
      B.build(Z80::RRr, B.lo(R)).addReg(B.lo(R));
      return true;
    }
    B.ld(Z80::A, B.hi(R));
    if (Arith) {
      B.accOp(Z80::RLCA); // The carry is the sign bit.
      B.accOp(Z80::RRCA);
    } else {
      B.clearCarry();
    }
    B.accOp(Z80::RRA);
    B.ld(B.hi(R), Z80::A);
    B.ld(Z80::A, B.lo(R));
    B.accOp(Z80::RRA);
    B.ld(B.lo(R), Z80::A);
    return true;
  }
  case Z80::SRL8:
    B.clearCarry();
    B.accOp(Z80::RRA);
    return true;
  case Z80::SRA8:
    B.accOp(Z80::RLCA);
    B.accOp(Z80::RRCA);
    B.accOp(Z80::RRA);
    return true;

  case Z80::SEXT16: {
    MCRegister Dst = MI.getOperand(0).getReg();
    B.ld(Z80::A, MI.getOperand(1).getReg());
    B.ld(B.lo(Dst), Z80::A);
    B.accOp(Z80::RLA);
    B.alu(Z80::SBCr, Z80::A);
    B.ld(B.hi(Dst), Z80::A);
    return true;
  }
  case Z80::ZEXT16: {
    MCRegister Dst = MI.getOperand(0).getReg();
    B.ld(B.lo(Dst), MI.getOperand(1).getReg());
    B.ldi(B.hi(Dst), 0);
    return true;
  }
  default:
    return false;
  }
}

static bool changesSP(const MachineInstr &MI, int &Amount) {
  switch (MI.getOpcode()) {
  case Z80::PUSHrp:
  case Z80::PUSHAF:
  case Z80::IXPUSH:
  case Z80::IYPUSH:
    Amount = 2;
    return true;
  case Z80::POPrp:
  case Z80::POPAF:
  case Z80::IXPOP:
  case Z80::IYPOP:
    Amount = -2;
    return true;
  case Z80::DECSP:
    Amount = 1;
    return true;
  case Z80::INCSP:
    Amount = -1;
    return true;
  default:
    return false;
  }
}

bool Z80ExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  const Z80Subtarget &STI = MF.getSubtarget<Z80Subtarget>();
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  const TargetRegisterInfo &TRI = *STI.getRegisterInfo();
  bool Changed = false;

  // SP at the start of each block, below its value after the prologue. A
  // call sequence can span blocks when an argument needs a select.
  DenseMap<const MachineBasicBlock *, int> DeltaIn;
  // With an IX frame, the prologue pushes IX below the frame.
  bool IXFrame = STI.getFrameLowering()->usesIXFrame(MF);
  if (IXFrame)
    DeltaIn[&MF.front()] = 2;

  ReversePostOrderTraversal<MachineFunction *> RPOT(&MF);
  for (MachineBasicBlock *MBB : RPOT) {
    // The registers live after each pseudo.
    DenseMap<const MachineInstr *, LiveRegUnits> LiveAfter;
    LiveRegUnits LR(TRI);
    LR.addLiveOuts(*MBB);
    for (MachineInstr &MI : llvm::reverse(*MBB)) {
      if (MI.isPseudo() && !MI.isMetaInstruction())
        LiveAfter.try_emplace(&MI, LR);
      LR.stepBackward(MI);
    }

    int Delta = DeltaIn.lookup(MBB);
    for (MachineInstr &MI : llvm::make_early_inc_range(*MBB)) {
      auto It = LiveAfter.find(&MI);
      if (It != LiveAfter.end()) {
        Builder B(MI, TII, TRI, It->second, Delta, STI.hasZ80(), IXFrame);
        if (expandMI(B, MI)) {
          MI.eraseFromParent();
          Changed = true;
          continue;
        }
      }
      int Amount;
      if (!MI.getFlag(MachineInstr::FrameSetup) &&
          !MI.getFlag(MachineInstr::FrameDestroy) && changesSP(MI, Amount))
        Delta += Amount;
    }
    for (MachineBasicBlock *Succ : MBB->successors())
      DeltaIn.try_emplace(Succ, Delta);
  }
  return Changed;
}
