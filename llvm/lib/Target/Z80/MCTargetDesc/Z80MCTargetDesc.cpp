//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80MCTargetDesc.h"
#include "TargetInfo/Z80TargetInfo.h"
#include "Z80InstPrinter.h"
#include "Z80MCAsmInfo.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "Z80GenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "Z80GenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "Z80GenRegisterInfo.inc"

static MCInstrInfo *createZ80MCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitZ80MCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createZ80MCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitZ80MCRegisterInfo(X, Z80::PC);
  return X;
}

static MCAsmInfo *createZ80MCAsmInfo(const MCRegisterInfo &MRI,
                                     const Triple &TT,
                                     const MCTargetOptions &Options) {
  MCAsmInfo *MAI = new Z80MCAsmInfo(TT, Options);
  // On entry the return address is at SP.
  MAI->addInitialFrameState(MCCFIInstruction::cfiDefCfa(
      nullptr, MRI.getDwarfRegNum(Z80::SP, true), 2));
  MAI->addInitialFrameState(MCCFIInstruction::createOffset(
      nullptr, MRI.getDwarfRegNum(Z80::PC, true), -2));
  return MAI;
}

static MCSubtargetInfo *createZ80MCSubtargetInfo(const Triple &TT,
                                                 StringRef CPU, StringRef FS) {
  if (CPU.empty() || CPU == "generic")
    CPU = TT.getArch() == Triple::i8080 ? "i8080" : "z80";
  return createZ80MCSubtargetInfoImpl(TT, CPU, /*TuneCPU=*/CPU, FS);
}

static MCInstPrinter *createZ80MCInstPrinter(const Triple &T,
                                             unsigned SyntaxVariant,
                                             const MCAsmInfo &MAI,
                                             const MCInstrInfo &MII,
                                             const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new Z80InstPrinter(MAI, MII, MRI);
  return nullptr;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeZ80TargetMC() {
  for (Target *T : {&getTheZ80Target(), &getTheI8080Target()}) {
    TargetRegistry::RegisterMCAsmInfo(*T, createZ80MCAsmInfo);
    TargetRegistry::RegisterMCInstrInfo(*T, createZ80MCInstrInfo);
    TargetRegistry::RegisterMCRegInfo(*T, createZ80MCRegisterInfo);
    TargetRegistry::RegisterMCSubtargetInfo(*T, createZ80MCSubtargetInfo);
    TargetRegistry::RegisterMCInstPrinter(*T, createZ80MCInstPrinter);
    TargetRegistry::RegisterMCCodeEmitter(*T, createZ80MCCodeEmitter);
    TargetRegistry::RegisterMCAsmBackend(*T, createZ80AsmBackend);
  }
}
