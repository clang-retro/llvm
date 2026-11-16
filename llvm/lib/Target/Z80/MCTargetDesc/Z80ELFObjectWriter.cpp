//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The relocations are GNU binutils'. e_flags is 1 (EF_Z80_MACH_Z80) for Z80
// code, and 0 for 8080 code.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/ADT/Twine.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {
class Z80ELFObjectWriter : public MCELFObjectTargetWriter {
public:
  Z80ELFObjectWriter()
      : MCELFObjectTargetWriter(/*Is64Bit=*/false, ELF::ELFOSABI_STANDALONE,
                                ELF::EM_Z80, /*HasRelocationAddend=*/true) {}

protected:
  unsigned getRelocType(const MCFixup &Fixup, const MCValue &,
                        bool IsPCRel) const override {
    switch (Fixup.getKind()) {
    case FK_Data_1:
      return IsPCRel ? ELF::R_Z80_8_PCREL : ELF::R_Z80_8;
    case FK_Data_2:
      return ELF::R_Z80_16;
    case FK_Data_4:
      return ELF::R_Z80_32;
    default:
      reportError(Fixup.getLoc(), "unsupported relocation");
      return ELF::R_Z80_NONE;
    }
  }
};
} // namespace

std::unique_ptr<MCObjectTargetWriter> llvm::createZ80ELFObjectWriter(bool IsZ80) {
  auto W = std::make_unique<Z80ELFObjectWriter>();
  return W;
}
