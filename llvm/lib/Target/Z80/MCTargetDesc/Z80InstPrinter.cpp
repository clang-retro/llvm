//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80InstPrinter.h"
#include "Z80MCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/Support/ErrorHandling.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

#include "Z80GenAsmWriter.inc"

void Z80InstPrinter::printRegName(raw_ostream &O, MCRegister Reg) {
  O << getRegisterName(Reg);
}

void Z80InstPrinter::printInst(const MCInst *MI, uint64_t Address,
                               StringRef Annot, const MCSubtargetInfo &STI,
                               raw_ostream &O) {
  printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

void Z80InstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                  raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    O << getRegisterName(Op.getReg());
  } else if (Op.isImm()) {
    O << Op.getImm();
  } else {
    assert(Op.isExpr() && "unknown operand kind in printOperand");
    MAI.printExpr(O, *Op.getExpr());
  }
}

// An index register displacement, with its sign: (ix+5), (iy-2).
void Z80InstPrinter::printDisp(const MCInst *MI, unsigned OpNo,
                               raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm()) {
    int64_t D = Op.getImm();
    if (D >= 0)
      O << '+';
    O << D;
    return;
  }
  O << '+';
  MAI.printExpr(O, *Op.getExpr());
}

const char *Z80InstPrinter::getCondCodeName(unsigned CC) {
  static const char *const Names[] = {"nz", "z", "nc", "c",
                                      "po", "pe", "p", "m"};
  assert(CC < 8 && "invalid condition code");
  return Names[CC];
}

void Z80InstPrinter::printCondCode(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &O) {
  O << getCondCodeName(MI->getOperand(OpNo).getImm());
}
