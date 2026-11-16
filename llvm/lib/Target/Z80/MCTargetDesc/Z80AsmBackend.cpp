//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
class Z80AsmBackend : public MCAsmBackend {
  bool IsZ80;

public:
  Z80AsmBackend(bool IsZ80)
      : MCAsmBackend(llvm::endianness::little), IsZ80(IsZ80) {}

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createZ80ELFObjectWriter(IsZ80);
  }

  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override;

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    OS.write_zeros(Count);
    return true;
  }
};
} // namespace

void Z80AsmBackend::applyFixup(const MCFragment &F, const MCFixup &Fixup,
                               const MCValue &Target, uint8_t *Data,
                               uint64_t Value, bool IsResolved) {
  maybeAddReloc(F, Fixup, Target, Value, IsResolved);
  unsigned Size;
  switch (Fixup.getKind()) {
  case FK_Data_1:
    Size = 1;
    if (IsResolved && Fixup.isPCRel()) {
      int64_t V = int64_t(Value);
      if (V < -128 || V > 127)
        getContext().reportError(Fixup.getLoc(),
                                 "relative jump target out of range");
    }
    break;
  case FK_Data_2:
    Size = 2;
    break;
  case FK_Data_4:
    Size = 4;
    break;
  case FK_Data_8:
    Size = 8;
    break;
  default:
    llvm_unreachable("unknown fixup kind");
  }
  for (unsigned I = 0; I != Size; ++I)
    Data[I] = uint8_t(Value >> (8 * I));
}

MCAsmBackend *llvm::createZ80AsmBackend(const Target &T,
                                        const MCSubtargetInfo &STI,
                                        const MCRegisterInfo &MRI,
                                        const MCTargetOptions &Options) {
  return new Z80AsmBackend(STI.hasFeature(Z80::FeatureZ80));
}
