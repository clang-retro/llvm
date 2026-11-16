//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file lowers machine instructions to MC and prints them.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80InstPrinter.h"
#include "TargetInfo/Z80TargetInfo.h"
#include "Z80.h"
#include "Z80TargetMachine.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {

class Z80AsmPrinter : public AsmPrinter {
public:
  static char ID;

  Z80AsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}

  StringRef getPassName() const override { return "Z80 Assembly Printer"; }

  void emitInstruction(const MachineInstr *MI) override;
  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &O) override;
  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode, raw_ostream &O) override;

private:
  MCOperand lowerSymbol(const MachineOperand &MO, MCSymbol *Sym) const;
  bool lowerOperand(const MachineOperand &MO, MCOperand &Out) const;
};

} // end anonymous namespace

MCOperand Z80AsmPrinter::lowerSymbol(const MachineOperand &MO,
                                     MCSymbol *Sym) const {
  const MCExpr *Expr = MCSymbolRefExpr::create(Sym, OutContext);
  if (!MO.isJTI() && !MO.isMBB() && MO.getOffset())
    Expr = MCBinaryExpr::createAdd(
        Expr, MCConstantExpr::create(MO.getOffset(), OutContext), OutContext);
  return MCOperand::createExpr(Expr);
}

bool Z80AsmPrinter::lowerOperand(const MachineOperand &MO,
                                 MCOperand &Out) const {
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    if (MO.isImplicit())
      return false;
    Out = MCOperand::createReg(MO.getReg());
    return true;
  case MachineOperand::MO_Immediate:
    Out = MCOperand::createImm(MO.getImm());
    return true;
  case MachineOperand::MO_MachineBasicBlock:
    Out = lowerSymbol(MO, MO.getMBB()->getSymbol());
    return true;
  case MachineOperand::MO_GlobalAddress:
    Out = lowerSymbol(MO, getSymbol(MO.getGlobal()));
    return true;
  case MachineOperand::MO_ExternalSymbol:
    Out = lowerSymbol(MO, GetExternalSymbolSymbol(MO.getSymbolName()));
    return true;
  case MachineOperand::MO_JumpTableIndex:
    Out = lowerSymbol(MO, GetJTISymbol(MO.getIndex()));
    return true;
  case MachineOperand::MO_ConstantPoolIndex:
    Out = lowerSymbol(MO, GetCPISymbol(MO.getIndex()));
    return true;
  case MachineOperand::MO_BlockAddress:
    Out = lowerSymbol(MO, GetBlockAddressSymbol(MO.getBlockAddress()));
    return true;
  case MachineOperand::MO_RegisterMask:
    return false;
  default:
    llvm_unreachable("Z80: unknown operand type");
  }
}

void Z80AsmPrinter::emitInstruction(const MachineInstr *MI) {
  Z80_MC::verifyInstructionPredicates(MI->getOpcode(),
                                      getSubtargetInfo().getFeatureBits());
  MCInst Inst;
  Inst.setOpcode(MI->getOpcode());
  for (const MachineOperand &MO : MI->operands()) {
    MCOperand Op;
    if (lowerOperand(MO, Op))
      Inst.addOperand(Op);
  }
  EmitToStreamer(*OutStreamer, Inst);
}

bool Z80AsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                    const char *ExtraCode, raw_ostream &O) {
  if (ExtraCode && ExtraCode[0])
    return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, O);
  const MachineOperand &MO = MI->getOperand(OpNo);
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    O << StringRef(Z80InstPrinter::getRegisterName(MO.getReg())).lower();
    return false;
  case MachineOperand::MO_Immediate:
    O << MO.getImm();
    return false;
  case MachineOperand::MO_GlobalAddress:
    PrintSymbolOperand(MO, O);
    return false;
  default:
    return true;
  }
}

bool Z80AsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                          unsigned OpNo,
                                          const char *ExtraCode,
                                          raw_ostream &O) {
  if (ExtraCode && ExtraCode[0])
    return true;
  const MachineOperand &MO = MI->getOperand(OpNo);
  if (!MO.isReg())
    return true;
  O << '(' << StringRef(Z80InstPrinter::getRegisterName(MO.getReg())).lower()
    << ')';
  return false;
}

char Z80AsmPrinter::ID = 0;

INITIALIZE_PASS(Z80AsmPrinter, "z80-asm-printer", "Z80 Assembly Printer",
                false, false)

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeZ80AsmPrinter() {
  RegisterAsmPrinter<Z80AsmPrinter> X(getTheZ80Target());
  RegisterAsmPrinter<Z80AsmPrinter> Y(getTheI8080Target());
}
