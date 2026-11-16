//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Provides the Z80 and 8080 MC descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_MCTARGETDESC_Z80MCTARGETDESC_H
#define LLVM_LIB_TARGET_Z80_MCTARGETDESC_Z80MCTARGETDESC_H

#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;
class Target;

MCCodeEmitter *createZ80MCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx);

MCAsmBackend *createZ80AsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                  const MCRegisterInfo &MRI,
                                  const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter> createZ80ELFObjectWriter(bool IsZ80);

namespace Z80 {
/// The condition codes, in the encoding of the cc field of JP, CALL and RET.
enum CondCode {
  COND_NZ = 0,
  COND_Z = 1,
  COND_NC = 2,
  COND_C = 3,
  COND_PO = 4,
  COND_PE = 5,
  COND_P = 6,
  COND_M = 7,
  COND_INVALID
};

inline CondCode getOppositeCondition(CondCode CC) {
  return CondCode(CC ^ 1);
}
} // namespace Z80
} // namespace llvm

#define GET_REGINFO_ENUM
#include "Z80GenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "Z80GenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "Z80GenSubtargetInfo.inc"

#endif
