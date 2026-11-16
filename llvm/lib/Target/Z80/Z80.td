//===-- Z80.td - Describe the Z80 and 8080 target ----------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The Z80 backend covers the Intel 8080 and the Zilog Z80. The 8080's
// instruction set is the base, and the Z80's additions (IX and IY, the CB,
// ED, DD and FD groups, and relative jumps) are a subtarget feature.
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

def FeatureZ80 : SubtargetFeature<"z80", "HasZ80", "true",
                                  "Zilog Z80 instructions">;

def : ProcessorModel<"i8080", NoSchedModel, []>;
def : ProcessorModel<"i8085", NoSchedModel, []>;
def : ProcessorModel<"z80", NoSchedModel, [FeatureZ80]>;

include "Z80RegisterInfo.td"
include "Z80InstrInfo.td"

defm : RemapAllTargetPseudoPointerOperands<GR16>;

// BC survives a call (and IX, which is reserved for the frame).
def CSR_Z80 : CalleeSavedRegs<(add BC)>;

def Z80InstrInfo : InstrInfo;

def Z80AsmParser : AsmParser {
  let ShouldEmitMatchRegisterName = 0;
}

def Z80AsmParserVariant : AsmParserVariant {
  // "(hl)", "(ix+5)" and "(1234h)" are separate tokens.
  let TokenizingCharacters = "()";
}

def Z80AsmWriter : AsmWriter {
  int PassSubtarget = 0;
}

def Z80 : Target {
  let InstructionSet = Z80InstrInfo;
  let AssemblyParsers = [Z80AsmParser];
  let AssemblyParserVariants = [Z80AsmParserVariant];
  let AssemblyWriters = [Z80AsmWriter];
  let AllowRegisterRenaming = 1;
}
