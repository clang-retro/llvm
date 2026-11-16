//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Encodes the Z80 and 8080 instructions. TableGen gives the bytes most
// significant first (see Z80InstrFormats.td).
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"

#define DEBUG_TYPE "mccodeemitter"

namespace llvm {

class Z80MCCodeEmitter : public MCCodeEmitter {
  MCContext &Ctx;
  const MCInstrInfo &MCII;

  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

  unsigned getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  unsigned getSize(const MCInst &MI) const {
    return MCII.get(MI.getOpcode()).getSize();
  }

  /// An 8-bit immediate in the last byte.
  unsigned getImm8OpValue(const MCInst &MI, unsigned OpNo,
                          SmallVectorImpl<MCFixup> &Fixups,
                          const MCSubtargetInfo &STI) const {
    const MCOperand &MO = MI.getOperand(OpNo);
    if (MO.isImm())
      return MO.getImm() & 0xff;
    Fixups.push_back(MCFixup::create(getSize(MI) - 1, MO.getExpr(), FK_Data_1));
    return 0;
  }

  /// A 16-bit immediate or address in the last two bytes.
  unsigned getImm16OpValue(const MCInst &MI, unsigned OpNo,
                           SmallVectorImpl<MCFixup> &Fixups,
                           const MCSubtargetInfo &STI) const {
    const MCOperand &MO = MI.getOperand(OpNo);
    if (MO.isImm())
      return MO.getImm() & 0xffff;
    Fixups.push_back(MCFixup::create(getSize(MI) - 2, MO.getExpr(), FK_Data_2));
    return 0;
  }

  /// The displacement of (IX+d), after the prefix and the opcode.
  unsigned getDisp8OpValue(const MCInst &MI, unsigned OpNo,
                           SmallVectorImpl<MCFixup> &Fixups,
                           const MCSubtargetInfo &STI) const {
    const MCOperand &MO = MI.getOperand(OpNo);
    if (MO.isImm())
      return MO.getImm() & 0xff;
    Fixups.push_back(MCFixup::create(2, MO.getExpr(), FK_Data_1));
    return 0;
  }

  /// The target of JR and DJNZ, relative to the next instruction, in the
  /// last byte. The fixup is relative to its own byte, one before.
  unsigned getPCRel8OpValue(const MCInst &MI, unsigned OpNo,
                            SmallVectorImpl<MCFixup> &Fixups,
                            const MCSubtargetInfo &STI) const {
    const MCOperand &MO = MI.getOperand(OpNo);
    if (MO.isImm())
      return MO.getImm() & 0xff;
    const MCExpr *Expr = MCBinaryExpr::createAdd(
        MO.getExpr(), MCConstantExpr::create(-1, Ctx), Ctx);
    Fixups.push_back(MCFixup::create(getSize(MI) - 1, Expr, FK_Data_1,
                                     /*PCRel=*/true));
    return 0;
  }

  unsigned getRstOpValue(const MCInst &MI, unsigned OpNo,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const {
    return (MI.getOperand(OpNo).getImm() >> 3) & 7;
  }

  unsigned getIMOpValue(const MCInst &MI, unsigned OpNo,
                        SmallVectorImpl<MCFixup> &Fixups,
                        const MCSubtargetInfo &STI) const {
    switch (MI.getOperand(OpNo).getImm()) {
    case 1:
      return 2;
    case 2:
      return 3;
    default:
      return 0;
    }
  }

public:
  Z80MCCodeEmitter(MCContext &Ctx, const MCInstrInfo &MCII)
      : Ctx(Ctx), MCII(MCII) {}

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override {
    unsigned Size = getSize(MI);
    uint64_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
    for (unsigned I = Size; I != 0; --I)
      CB.push_back(char((Bits >> (8 * (I - 1))) & 0xff));
  }
};

unsigned Z80MCCodeEmitter::getMachineOpValue(const MCInst &MI,
                                             const MCOperand &MO,
                                             SmallVectorImpl<MCFixup> &Fixups,
                                             const MCSubtargetInfo &STI) const {
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  if (MO.isImm())
    return MO.getImm();
  llvm_unreachable("expression operand without an encoder method");
}

MCCodeEmitter *createZ80MCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx) {
  return new Z80MCCodeEmitter(Ctx, MCII);
}

#include "Z80GenMCCodeEmitter.inc"

} // namespace llvm
