//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Many instructions take an operand or give their result in HL or in A. A
// value that lived in such a one-register class would hold HL or A for its
// whole life, and two of them could not be live together. So before
// register allocation each such operand gets a value of its own, copied
// from or to one of the wider class, which the register allocator can put
// anywhere. Z80RegisterInfo::shouldCoalesce keeps the copies.
//
//===----------------------------------------------------------------------===//

#include "Z80.h"
#include "Z80RegisterInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"

using namespace llvm;

#define DEBUG_TYPE "z80-split-narrow"
#define PASS_NAME "Z80 one-register operand splitting"

namespace {
class Z80SplitNarrow : public MachineFunctionPass {
public:
  static char ID;
  Z80SplitNarrow() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return PASS_NAME; }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};
} // end anonymous namespace

char Z80SplitNarrow::ID = 0;

INITIALIZE_PASS(Z80SplitNarrow, DEBUG_TYPE, PASS_NAME, false, false)

FunctionPass *llvm::createZ80SplitNarrowPass() { return new Z80SplitNarrow(); }

// The register class an operand of a single register class (HL or A)
// widens to.
static const TargetRegisterClass *widen(const TargetRegisterClass *RC) {
  if (Z80::GR16RegClass.hasSubClassEq(RC))
    return &Z80::GR16RegClass;
  if (Z80::GR8RegClass.hasSubClassEq(RC))
    return &Z80::GR8RegClass;
  return RC;
}

bool Z80SplitNarrow::runOnMachineFunction(MachineFunction &MF) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  bool Changed = false;

  // Whether an instruction gives a result in a one-register class there.
  auto NarrowDef = [&](const MachineOperand &Def) {
    const MachineInstr &DefMI = *Def.getParent();
    unsigned Idx = DefMI.getOperandNo(&Def);
    if (DefMI.isPreISelOpcode() || DefMI.isCopy() || DefMI.isPHI() ||
        Idx >= DefMI.getNumExplicitOperands())
      return false;
    const TargetRegisterClass *RC = TII.getRegClass(DefMI.getDesc(), Idx);
    return RC && RC->getNumRegs() == 1;
  };

  // Machine SSA optimizations fold copies, which narrows values to HL or A
  // for their whole life. Widen those back first.
  for (unsigned I = 0, E = MRI.getNumVirtRegs(); I != E; ++I) {
    Register Reg = Register::index2VirtReg(I);
    if (MRI.reg_nodbg_empty(Reg))
      continue;
    const TargetRegisterClass *RC = MRI.getRegClass(Reg);
    if (RC->getNumRegs() != 1 || widen(RC) == RC)
      continue;
    if (llvm::none_of(MRI.def_operands(Reg), NarrowDef)) {
      MRI.setRegClass(Reg, widen(RC));
      Changed = true;
    }
  }

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
      if (MI.isPreISelOpcode() || MI.isCopy() || MI.isPHI() ||
          MI.isRegSequence() || MI.isInsertSubreg() ||
          MI.isExtractSubreg() || MI.isSubregToReg() || MI.isInlineAsm() ||
          MI.isImplicitDef())
        continue;
      const MCInstrDesc &Desc = MI.getDesc();
      for (unsigned I = 0, E = MI.getNumExplicitOperands(); I != E; ++I) {
        MachineOperand &MO = MI.getOperand(I);
        if (!MO.isReg() || !MO.getReg().isVirtual() || MO.getSubReg())
          continue;
        const TargetRegisterClass *RC = TII.getRegClass(Desc, I);
        if (!RC || RC->getNumRegs() != 1)
          continue;
        Register Reg = MO.getReg();
        if (MO.isUse()) {
          // A tied use is copied too: the result gets its own value below.
          Register New = MRI.createVirtualRegister(RC);
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), New)
              .addReg(Reg);
          MO.setReg(New);
          Changed = true;
          continue;
        }
        // A result: the other uses read a copy of it.
        Register Wide = MRI.createVirtualRegister(widen(RC));
        MachineInstr *Copy =
            BuildMI(MBB, std::next(MI.getIterator()), MI.getDebugLoc(),
                    TII.get(TargetOpcode::COPY), Wide)
                .addReg(Reg);
        for (MachineOperand &Use :
             llvm::make_early_inc_range(MRI.use_operands(Reg)))
          if (Use.getParent() != Copy)
            Use.setReg(Wide);
        Changed = true;
      }
    }
  }
  return Changed;
}
