//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The calling convention: the first two 16-bit argument parts go in HL and
// DE (an 8-bit one in L or E), the rest are pushed, last first, so the first
// is at SP+2 on entry. Variadic arguments are always pushed. The caller pops
// the arguments. Results come back in HL and DE (L and E for 8-bit parts);
// larger ones are returned through a pointer passed as the first argument.
// No register survives a call.
//
//===----------------------------------------------------------------------===//

#include "Z80ISelLowering.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80MachineFunctionInfo.h"
#include "Z80Subtarget.h"
#include "Z80TargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "z80-lower"

Z80TargetLowering::Z80TargetLowering(const TargetMachine &TM,
                                     const Z80Subtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  addRegisterClass(MVT::i8, &Z80::GR8RegClass);
  addRegisterClass(MVT::i16, &Z80::GR16RegClass);
  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(Z80::SP);
  setBooleanContents(ZeroOrOneBooleanContent);
  setSchedulingPreference(Sched::RegPressure);
  setMinFunctionAlignment(Align(1));
  setPrefFunctionAlignment(Align(1));
  setMaxAtomicSizeInBitsSupported(0);
  setMinimumJumpTableEntries(8);

  for (MVT VT : {MVT::i8, MVT::i16}) {
    setOperationAction(ISD::BR_CC, VT, Custom);
    setOperationAction(ISD::SELECT_CC, VT, Custom);
    setOperationAction(ISD::SETCC, VT, Expand);
    setOperationAction(ISD::SELECT, VT, Expand);
    setOperationAction(ISD::SHL, VT, Custom);
    setOperationAction(ISD::SRL, VT, Custom);
    setOperationAction(ISD::SRA, VT, Custom);
    for (unsigned Op : {ISD::ROTL, ISD::ROTR, ISD::FSHL, ISD::FSHR,
                        ISD::SHL_PARTS, ISD::SRL_PARTS, ISD::SRA_PARTS,
                        ISD::MULHS, ISD::MULHU, ISD::SMUL_LOHI,
                        ISD::UMUL_LOHI, ISD::SDIVREM, ISD::UDIVREM,
                        ISD::CTPOP, ISD::CTLZ, ISD::CTTZ, ISD::BSWAP,
                        ISD::BITREVERSE, ISD::UADDO, ISD::USUBO,
                        ISD::SADDO, ISD::SSUBO, ISD::UADDO_CARRY,
                        ISD::USUBO_CARRY, ISD::SADDO_CARRY,
                        ISD::SSUBO_CARRY, ISD::UMULO, ISD::SMULO,
                        ISD::ABS, ISD::SMIN, ISD::SMAX, ISD::UMIN,
                        ISD::UMAX, ISD::SIGN_EXTEND_INREG,
                        ISD::DYNAMIC_STACKALLOC})
      setOperationAction(Op, VT, Expand);
  }
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  setOperationAction(ISD::ADDC, MVT::i8, Expand);
  setOperationAction(ISD::ADDE, MVT::i8, Expand);
  setOperationAction(ISD::SUBC, MVT::i8, Expand);
  setOperationAction(ISD::SUBE, MVT::i8, Expand);
  // Wider additions and subtractions carry from one 16-bit part to the next.
  for (unsigned Op : {ISD::ADDC, ISD::ADDE, ISD::SUBC, ISD::SUBE})
    setOperationAction(Op, MVT::i16, Legal);

  setOperationAction(ISD::MUL, MVT::i8, Promote);
  setOperationAction(ISD::MUL, MVT::i16, LibCall);
  for (unsigned Op : {ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM}) {
    setOperationAction(Op, MVT::i8, Promote);
    setOperationAction(Op, MVT::i16, LibCall);
  }

  for (unsigned Op : {ISD::GlobalAddress, ISD::ExternalSymbol,
                      ISD::BlockAddress, ISD::JumpTable, ISD::ConstantPool})
    setOperationAction(Op, MVT::i16, Custom);

  setOperationAction(ISD::BR_JT, MVT::Other, Expand);
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Expand);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);

  for (MVT VT : MVT::integer_valuetypes()) {
    setLoadExtAction(ISD::EXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::ZEXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i1, Promote);
  }
  setLoadExtAction(ISD::EXTLOAD, MVT::i16, MVT::i8, Expand);
  setLoadExtAction(ISD::ZEXTLOAD, MVT::i16, MVT::i8, Expand);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i16, MVT::i8, Expand);
  setTruncStoreAction(MVT::i16, MVT::i8, Expand);
}

//===----------------------------------------------------------------------===//
// Calling convention
//===----------------------------------------------------------------------===//

static const MCPhysReg ArgRegs16[] = {Z80::HL, Z80::DE};
static const MCPhysReg ArgRegs8[] = {Z80::L, Z80::E};

// Assign the argument parts: fixed ones to HL and DE while they last, the
// rest to 2-byte stack slots.
template <typename ArgT>
static void analyzeArguments(CCState &State,
                             SmallVectorImpl<CCValAssign> &Locs,
                             const SmallVectorImpl<ArgT> &Args) {
  unsigned NextReg = 0;
  for (unsigned I = 0, E = Args.size(); I != E; ++I) {
    MVT VT = Args[I].VT;
    if (VT != MVT::i8 && VT != MVT::i16)
      report_fatal_error("Z80: unsupported argument type");
    if (!Args[I].Flags.isVarArg() && NextReg < 2) {
      MCPhysReg Reg =
          (VT == MVT::i8 ? ArgRegs8 : ArgRegs16)[NextReg++];
      State.addLoc(CCValAssign::getReg(I, VT, Reg, VT, CCValAssign::Full));
      continue;
    }
    unsigned Off = State.AllocateStack(2, Align(1));
    State.addLoc(CCValAssign::getMem(I, VT, Off, VT, CCValAssign::Full));
  }
}

// Results: up to two parts, in HL and DE.
template <typename ArgT>
static bool analyzeReturn(CCState &State, const SmallVectorImpl<ArgT> &Rets) {
  if (Rets.size() > 2)
    return false;
  for (unsigned I = 0, E = Rets.size(); I != E; ++I) {
    MVT VT = Rets[I].VT;
    if (VT != MVT::i8 && VT != MVT::i16)
      return false;
    MCPhysReg Reg = (VT == MVT::i8 ? ArgRegs8 : ArgRegs16)[I];
    State.addLoc(CCValAssign::getReg(I, VT, Reg, VT, CCValAssign::Full));
  }
  return true;
}

bool Z80TargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context,
    const Type *RetTy) const {
  SmallVector<CCValAssign, 4> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);
  return analyzeReturn(CCInfo, Outs);
}

SDValue Z80TargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  auto *FuncInfo = MF.getInfo<Z80MachineFunctionInfo>();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  analyzeArguments(CCInfo, ArgLocs, Ins);

  for (CCValAssign &VA : ArgLocs) {
    MVT VT = VA.getLocVT();
    if (VA.isRegLoc()) {
      Register VReg = MRI.createVirtualRegister(
          VT == MVT::i8 ? &Z80::GR8RegClass : &Z80::GR16RegClass);
      MRI.addLiveIn(VA.getLocReg(), VReg);
      InVals.push_back(DAG.getCopyFromReg(Chain, DL, VReg, VT));
      continue;
    }
    // The return address is below the stack arguments.
    int FI = MFI.CreateFixedObject(VT.getStoreSize(),
                                   2 + VA.getLocMemOffset(), true);
    SDValue FIN = DAG.getFrameIndex(FI, MVT::i16);
    InVals.push_back(DAG.getLoad(VT, DL, Chain, FIN,
                                 MachinePointerInfo::getFixedStack(MF, FI)));
  }

  if (IsVarArg)
    FuncInfo->setVarArgsFrameIndex(
        MFI.CreateFixedObject(1, 2 + CCInfo.getStackSize(), true));

  return Chain;
}

SDValue Z80TargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                     SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  MachineFunction &MF = DAG.getMachineFunction();
  CLI.IsTailCall = false;

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CLI.CallConv, CLI.IsVarArg, MF, ArgLocs, *DAG.getContext());
  analyzeArguments(CCInfo, ArgLocs, Outs);
  unsigned NumBytes = CCInfo.getStackSize();

  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  // Push the stack arguments, the last first.
  SmallVector<std::pair<unsigned, SDValue>, 8> StackArgs;
  SmallVector<std::pair<MCPhysReg, SDValue>, 2> RegsToPass;
  for (unsigned I = 0, E = ArgLocs.size(); I != E; ++I) {
    CCValAssign &VA = ArgLocs[I];
    SDValue Arg = OutVals[I];
    if (VA.isRegLoc()) {
      RegsToPass.push_back({VA.getLocReg(), Arg});
      continue;
    }
    if (Arg.getValueType() == MVT::i8)
      Arg = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i16, Arg);
    StackArgs.push_back({VA.getLocMemOffset(), Arg});
  }
  llvm::sort(StackArgs, [](const auto &A, const auto &B) {
    return A.first > B.first;
  });
  for (auto &[Off, Arg] : StackArgs)
    Chain = DAG.getNode(Z80ISD::PUSH, DL, MVT::Other, Chain, Arg);

  // An indirect call goes through __z80_call_bc, which jumps to BC.
  bool Indirect = false;
  if (auto *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i16,
                                        G->getOffset());
  } else if (auto *ES = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    Callee = DAG.getTargetExternalSymbol(ES->getSymbol(), MVT::i16);
  } else {
    RegsToPass.push_back({Z80::BC, Callee});
    Callee = DAG.getTargetExternalSymbol("__z80_call_bc", MVT::i16);
    Indirect = true;
  }
  (void)Indirect;

  SDValue InGlue;
  for (auto &[Reg, Val] : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg, Val, InGlue);
    InGlue = Chain.getValue(1);
  }

  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  for (auto &[Reg, Val] : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg, Val.getValueType()));
  const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
  Ops.push_back(DAG.getRegisterMask(TRI->getCallPreservedMask(MF, CLI.CallConv)));
  if (InGlue.getNode())
    Ops.push_back(InGlue);

  Chain = DAG.getNode(Z80ISD::CALL, DL, DAG.getVTList(MVT::Other, MVT::Glue),
                      Ops);
  InGlue = Chain.getValue(1);
  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, InGlue, DL);
  InGlue = Chain.getValue(1);

  SmallVector<CCValAssign, 4> RVLocs;
  CCState RetInfo(CLI.CallConv, CLI.IsVarArg, MF, RVLocs, *DAG.getContext());
  if (!analyzeReturn(RetInfo, Ins))
    report_fatal_error("Z80: unsupported call result type");
  for (CCValAssign &VA : RVLocs) {
    Chain = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getValVT(), InGlue)
                .getValue(1);
    InGlue = Chain.getValue(2);
    InVals.push_back(Chain.getValue(0));
  }
  return Chain;
}

SDValue
Z80TargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::OutputArg> &Outs,
                               const SmallVectorImpl<SDValue> &OutVals,
                               const SDLoc &DL, SelectionDAG &DAG) const {
  SmallVector<CCValAssign, 4> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());
  if (!analyzeReturn(CCInfo, Outs))
    report_fatal_error("Z80: unsupported return type");

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);
  for (unsigned I = 0, E = RVLocs.size(); I != E; ++I) {
    CCValAssign &VA = RVLocs[I];
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), OutVals[I], Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }
  RetOps[0] = Chain;
  if (Glue.getNode())
    RetOps.push_back(Glue);
  return DAG.getNode(Z80ISD::RET, DL, MVT::Other, RetOps);
}

//===----------------------------------------------------------------------===//
// Custom lowering
//===----------------------------------------------------------------------===//

SDValue Z80TargetLowering::LowerOperation(SDValue Op, SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::SHL:
  case ISD::SRL:
  case ISD::SRA:
    return lowerShift(Op, DAG);
  case ISD::GlobalAddress:
  case ISD::ExternalSymbol:
  case ISD::BlockAddress:
  case ISD::JumpTable:
  case ISD::ConstantPool:
    return lowerAddress(Op, DAG);
  case ISD::BR_CC:
    return lowerBR_CC(Op, DAG);
  case ISD::SELECT_CC:
    return lowerSELECT_CC(Op, DAG);
  case ISD::VASTART:
    return lowerVASTART(Op, DAG);
  default:
    llvm_unreachable("Z80: unexpected operation to lower");
  }
}

void Z80TargetLowering::ReplaceNodeResults(SDNode *N,
                                           SmallVectorImpl<SDValue> &Results,
                                           SelectionDAG &DAG) const {
  // Nothing: the illegal types are expanded the generic way.
}

SDValue Z80TargetLowering::lowerAddress(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT VT = Op.getValueType();
  SDValue Result;
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress: {
    auto *G = cast<GlobalAddressSDNode>(Op);
    Result = DAG.getTargetGlobalAddress(G->getGlobal(), DL, VT, G->getOffset());
    break;
  }
  case ISD::ExternalSymbol:
    Result = DAG.getTargetExternalSymbol(
        cast<ExternalSymbolSDNode>(Op)->getSymbol(), VT);
    break;
  case ISD::BlockAddress: {
    auto *BA = cast<BlockAddressSDNode>(Op);
    Result = DAG.getTargetBlockAddress(BA->getBlockAddress(), VT,
                                       BA->getOffset());
    break;
  }
  case ISD::JumpTable:
    Result = DAG.getTargetJumpTable(cast<JumpTableSDNode>(Op)->getIndex(), VT);
    break;
  case ISD::ConstantPool: {
    auto *CP = cast<ConstantPoolSDNode>(Op);
    Result = CP->isMachineConstantPoolEntry()
                 ? DAG.getTargetConstantPool(CP->getMachineCPVal(), VT,
                                             CP->getAlign(), CP->getOffset())
                 : DAG.getTargetConstantPool(CP->getConstVal(), VT,
                                             CP->getAlign(), CP->getOffset());
    break;
  }
  }
  return DAG.getNode(Z80ISD::WRAPPER, DL, VT, Result);
}

// Shifts by a constant become shifts by one bit, or byte moves; the others
// call the 16-bit libgcc helpers.
SDValue Z80TargetLowering::lowerShift(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  unsigned Opc = Op.getOpcode();
  EVT VT = Op.getValueType();
  SDValue X = Op.getOperand(0);
  auto *CN = dyn_cast<ConstantSDNode>(Op.getOperand(1));

  if (!CN) {
    RTLIB::Libcall LC = Opc == ISD::SHL   ? RTLIB::SHL_I16
                        : Opc == ISD::SRL ? RTLIB::SRL_I16
                                          : RTLIB::SRA_I16;
    SDValue X16 = X;
    if (VT == MVT::i8)
      X16 = DAG.getNode(Opc == ISD::SRA ? ISD::SIGN_EXTEND : ISD::ZERO_EXTEND,
                        DL, MVT::i16, X);
    SDValue Amt = DAG.getZExtOrTrunc(Op.getOperand(1), DL, MVT::i16);
    MakeLibCallOptions CallOptions;
    SDValue R =
        makeLibCall(DAG, LC, MVT::i16, {X16, Amt}, CallOptions, DL).first;
    return VT == MVT::i8 ? DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, R) : R;
  }

  unsigned Bits = VT.getSizeInBits();
  unsigned Amt = CN->getZExtValue() & (Bits - 1);
  unsigned One = Opc == ISD::SHL   ? Z80ISD::SHL1
                 : Opc == ISD::SRL ? Z80ISD::SRL1
                                   : Z80ISD::SRA1;
  auto repeat = [&](SDValue V, unsigned N) {
    for (unsigned I = 0; I < N; ++I)
      V = DAG.getNode(One, DL, V.getValueType(), V);
    return V;
  };

  // x >> (Bits-1), arithmetic, is all copies of the sign bit: the high byte
  // of the byte with the sign bit sign-extended (RLA; SBC A,A).
  if (Opc == ISD::SRA && Amt == Bits - 1) {
    SDValue Hi = VT == MVT::i16 ? DAG.getNode(Z80ISD::HI, DL, MVT::i8, X) : X;
    SDValue M = DAG.getNode(
        Z80ISD::HI, DL, MVT::i8,
        DAG.getNode(ISD::SIGN_EXTEND, DL, MVT::i16, Hi));
    return VT == MVT::i8 ? M
                         : DAG.getNode(Z80ISD::COMBINE, DL, MVT::i16, M, M);
  }

  if (VT == MVT::i16 && Amt >= 8) {
    SDValue Zero = DAG.getConstant(0, DL, MVT::i8);
    if (Opc == ISD::SHL) {
      SDValue Lo = DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, X);
      return DAG.getNode(Z80ISD::COMBINE, DL, MVT::i16, Zero,
                         repeat(Lo, Amt - 8));
    }
    SDValue Hi = DAG.getNode(Z80ISD::HI, DL, MVT::i8, X);
    Hi = repeat(Hi, Amt - 8);
    if (Opc == ISD::SRL)
      return DAG.getNode(Z80ISD::COMBINE, DL, MVT::i16, Hi, Zero);
    return DAG.getNode(ISD::SIGN_EXTEND, DL, MVT::i16, Hi);
  }
  return repeat(X, Amt);
}

// Compare LHS and RHS for CC, setting Z80CC to the condition of the flags
// that holds when the comparison does. Returns the glue of the flags.
SDValue Z80TargetLowering::emitCompare(SDValue LHS, SDValue RHS,
                                       ISD::CondCode CC, SDValue &Z80CC,
                                       const SDLoc &DL,
                                       SelectionDAG &DAG) const {
  EVT VT = LHS.getValueType();
  assert((VT == MVT::i8 || VT == MVT::i16) && "illegal comparison type");

  // x < 0 and x >= 0 (and x <= -1, x > -1) need only the sign bit: OR A
  // with the high byte sets the S flag from it.
  if (auto *C = dyn_cast<ConstantSDNode>(RHS)) {
    const APInt &CV = C->getAPIntValue();
    bool Neg = (CC == ISD::SETLT && CV.isZero()) ||
               (CC == ISD::SETLE && CV.isAllOnes());
    bool NonNeg = (CC == ISD::SETGE && CV.isZero()) ||
                  (CC == ISD::SETGT && CV.isAllOnes());
    if (Neg || NonNeg) {
      SDValue V =
          VT == MVT::i16 ? DAG.getNode(Z80ISD::HI, DL, MVT::i8, LHS) : LHS;
      Z80CC = DAG.getTargetConstant(Neg ? Z80::COND_M : Z80::COND_P, DL,
                                    MVT::i8);
      return DAG.getNode(Z80ISD::TST, DL, MVT::Glue, V);
    }
  }

  // A signed comparison is the unsigned one of the values with their sign
  // bits flipped.
  if (isSignedIntSetCC(CC)) {
    SDValue Sign = DAG.getConstant(APInt::getSignMask(VT.getSizeInBits()), DL,
                                   VT);
    LHS = DAG.getNode(ISD::XOR, DL, VT, LHS, Sign);
    RHS = DAG.getNode(ISD::XOR, DL, VT, RHS, Sign);
    CC = ISD::getUnsignedSetCC(CC);
  }

  Z80::CondCode Cond;
  SDValue Glue;
  switch (CC) {
  case ISD::SETEQ:
  case ISD::SETNE: {
    Cond = CC == ISD::SETEQ ? Z80::COND_Z : Z80::COND_NZ;
    if (VT == MVT::i8) {
      // OR A is a byte shorter than CP 0.
      Glue = isNullConstant(RHS)
                 ? DAG.getNode(Z80ISD::TST, DL, MVT::Glue, LHS)
                 : DAG.getNode(Z80ISD::CMP, DL, MVT::Glue, LHS, RHS);
      break;
    }
    SDValue V = isNullConstant(RHS)   ? LHS
                : isNullConstant(LHS) ? RHS
                                      : DAG.getNode(ISD::XOR, DL, VT, LHS, RHS);
    Glue = DAG.getNode(Z80ISD::TST, DL, MVT::Glue, V);
    break;
  }
  case ISD::SETUGT:
  case ISD::SETULE: {
    // x > c is x >= c+1.
    auto *C = dyn_cast<ConstantSDNode>(RHS);
    if (C && !C->getAPIntValue().isMaxValue()) {
      RHS = DAG.getConstant(C->getAPIntValue() + 1, DL, VT);
      Cond = CC == ISD::SETUGT ? Z80::COND_NC : Z80::COND_C;
    } else {
      std::swap(LHS, RHS);
      Cond = CC == ISD::SETUGT ? Z80::COND_C : Z80::COND_NC;
    }
    Glue = DAG.getNode(Z80ISD::CMP, DL, MVT::Glue, LHS, RHS);
    break;
  }
  case ISD::SETULT:
  case ISD::SETUGE:
    Cond = CC == ISD::SETULT ? Z80::COND_C : Z80::COND_NC;
    Glue = DAG.getNode(Z80ISD::CMP, DL, MVT::Glue, LHS, RHS);
    break;
  default:
    llvm_unreachable("Z80: unexpected condition");
  }
  Z80CC = DAG.getTargetConstant(Cond, DL, MVT::i8);
  return Glue;
}

SDValue Z80TargetLowering::lowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue Z80CC;
  SDValue Glue =
      emitCompare(Op.getOperand(2), Op.getOperand(3), CC, Z80CC, DL, DAG);
  return DAG.getNode(Z80ISD::BRCOND, DL, MVT::Other, Chain, Op.getOperand(4),
                     Z80CC, Glue);
}

SDValue Z80TargetLowering::lowerSELECT_CC(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
  SDValue Z80CC;
  SDValue Glue =
      emitCompare(Op.getOperand(0), Op.getOperand(1), CC, Z80CC, DL, DAG);
  return DAG.getNode(Z80ISD::SELECT_CC, DL, Op.getValueType(),
                     Op.getOperand(2), Op.getOperand(3), Z80CC, Glue);
}

SDValue Z80TargetLowering::lowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  auto *FuncInfo = MF.getInfo<Z80MachineFunctionInfo>();
  SDLoc DL(Op);
  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(), MVT::i16);
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, FI, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

//===----------------------------------------------------------------------===//
// Custom inserters
//===----------------------------------------------------------------------===//

MachineBasicBlock *
Z80TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                               MachineBasicBlock *BB) const {
  assert((MI.getOpcode() == Z80::SELECT8 || MI.getOpcode() == Z80::SELECT16) &&
         "unexpected instruction to insert");
  const TargetInstrInfo &TII = *Subtarget.getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();

  // ThisMBB:
  //   jp cc, SinkMBB
  // FalseMBB:
  // SinkMBB:
  //   dst = phi [t, ThisMBB], [f, FalseMBB]
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineFunction::iterator It = ++BB->getIterator();
  MachineFunction *MF = BB->getParent();
  MachineBasicBlock *ThisMBB = BB;
  MachineBasicBlock *FalseMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *SinkMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MF->insert(It, FalseMBB);
  MF->insert(It, SinkMBB);

  SinkMBB->splice(SinkMBB->begin(), BB,
                  std::next(MachineBasicBlock::iterator(MI)), BB->end());
  SinkMBB->transferSuccessorsAndUpdatePHIs(BB);
  BB->addSuccessor(FalseMBB);
  BB->addSuccessor(SinkMBB);

  BuildMI(BB, DL, TII.get(Z80::JPcc))
      .addImm(MI.getOperand(3).getImm())
      .addMBB(SinkMBB);

  FalseMBB->addSuccessor(SinkMBB);

  BuildMI(*SinkMBB, SinkMBB->begin(), DL, TII.get(Z80::PHI),
          MI.getOperand(0).getReg())
      .addReg(MI.getOperand(1).getReg())
      .addMBB(ThisMBB)
      .addReg(MI.getOperand(2).getReg())
      .addMBB(FalseMBB);

  MI.eraseFromParent();
  return SinkMBB;
}

//===----------------------------------------------------------------------===//
// Miscellaneous
//===----------------------------------------------------------------------===//

bool Z80TargetLowering::isTruncateFree(Type *Ty1, Type *Ty2) const {
  if (!Ty1->isIntegerTy() || !Ty2->isIntegerTy())
    return false;
  return Ty1->getPrimitiveSizeInBits() > Ty2->getPrimitiveSizeInBits();
}

bool Z80TargetLowering::isTruncateFree(EVT VT1, EVT VT2) const {
  if (!VT1.isInteger() || !VT2.isInteger())
    return false;
  return VT1.getSizeInBits() > VT2.getSizeInBits();
}

Z80TargetLowering::ConstraintType
Z80TargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
    case 'a':
      return C_RegisterClass;
    default:
      break;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
Z80TargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                StringRef Constraint,
                                                MVT VT) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      if (VT == MVT::i8)
        return {0U, &Z80::GR8RegClass};
      return {0U, &Z80::GR16RegClass};
    case 'a':
      return {Z80::A, &Z80::ACCRegClass};
    default:
      break;
    }
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}
