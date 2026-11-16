//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the instruction selector for the Z80 and the 8080.
//
//===----------------------------------------------------------------------===//

#include "Z80.h"
#include "Z80TargetMachine.h"
#include "llvm/CodeGen/SelectionDAGISel.h"

using namespace llvm;

#define DEBUG_TYPE "z80-isel"
#define PASS_NAME "Z80 DAG->DAG Pattern Instruction Selection"

namespace {

class Z80DAGToDAGISel : public SelectionDAGISel {
  const Z80Subtarget *Subtarget = nullptr;

public:
  Z80DAGToDAGISel(Z80TargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISel(TM, OptLevel) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    Subtarget = &MF.getSubtarget<Z80Subtarget>();
    return SelectionDAGISel::runOnMachineFunction(MF);
  }

  void Select(SDNode *N) override;

  bool SelectInlineAsmMemoryOperand(const SDValue &Op,
                                    InlineAsm::ConstraintCode ConstraintID,
                                    std::vector<SDValue> &OutOps) override;

  // A stack object and an offset into it.
  bool SelectFrameAddr(SDValue N, SDValue &Base, SDValue &Offset);
  // A symbol or a constant address.
  bool SelectAbsAddr(SDValue N, SDValue &Out);

#include "Z80GenDAGISel.inc"
};

class Z80DAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;
  Z80DAGToDAGISelLegacy(Z80TargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISelLegacy(
            ID, std::make_unique<Z80DAGToDAGISel>(TM, OptLevel)) {}
};

} // end anonymous namespace

char Z80DAGToDAGISelLegacy::ID = 0;

INITIALIZE_PASS(Z80DAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false, false)

FunctionPass *llvm::createZ80ISelDag(Z80TargetMachine &TM,
                                     CodeGenOptLevel OptLevel) {
  return new Z80DAGToDAGISelLegacy(TM, OptLevel);
}

bool Z80DAGToDAGISel::SelectFrameAddr(SDValue N, SDValue &Base,
                                      SDValue &Offset) {
  SDLoc DL(N);
  if (auto *FIN = dyn_cast<FrameIndexSDNode>(N)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i16);
    Offset = CurDAG->getTargetConstant(0, DL, MVT::i16);
    return true;
  }
  if (CurDAG->isBaseWithConstantOffset(N)) {
    if (auto *FIN = dyn_cast<FrameIndexSDNode>(N.getOperand(0))) {
      int64_t Off = cast<ConstantSDNode>(N.getOperand(1))->getSExtValue();
      Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i16);
      Offset = CurDAG->getSignedTargetConstant(Off, DL, MVT::i16);
      return true;
    }
  }
  return false;
}

bool Z80DAGToDAGISel::SelectAbsAddr(SDValue N, SDValue &Out) {
  if (N.getOpcode() == Z80ISD::WRAPPER) {
    SDValue Op = N.getOperand(0);
    switch (Op.getOpcode()) {
    case ISD::TargetGlobalAddress:
    case ISD::TargetExternalSymbol:
    case ISD::TargetBlockAddress:
    case ISD::TargetConstantPool:
      Out = Op;
      return true;
    default:
      return false;
    }
  }
  if (auto *C = dyn_cast<ConstantSDNode>(N)) {
    Out = CurDAG->getTargetConstant(C->getZExtValue() & 0xFFFF, SDLoc(N),
                                    MVT::i16);
    return true;
  }
  return false;
}

void Z80DAGToDAGISel::Select(SDNode *N) {
  if (N->isMachineOpcode()) {
    N->setNodeId(-1);
    return;
  }

  if (N->getOpcode() == ISD::FrameIndex) {
    SDLoc DL(N);
    int FI = cast<FrameIndexSDNode>(N)->getIndex();
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, MVT::i16);
    SDValue Zero = CurDAG->getTargetConstant(0, DL, MVT::i16);
    CurDAG->SelectNodeTo(N, Z80::LEAfi, MVT::i16, TFI, Zero);
    return;
  }

  SelectCode(N);
}

bool Z80DAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, InlineAsm::ConstraintCode ConstraintID,
    std::vector<SDValue> &OutOps) {
  switch (ConstraintID) {
  case InlineAsm::ConstraintCode::m:
    // The address goes in a register; the template writes (hl) and so on.
    OutOps.push_back(Op);
    return false;
  default:
    return true;
  }
}
