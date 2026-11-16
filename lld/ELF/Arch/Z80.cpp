//===- Z80.cpp ------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The Zilog Z80 and the Intel 8080 use the relocations of the GNU binutils
// Z80 port. Programs are static: a CP/M .COM file is the flat image of the
// output, linked at 0x100.
//
//===----------------------------------------------------------------------===//

#include "Symbols.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
class Z80 final : public TargetInfo {
public:
  Z80(Ctx &);
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};
} // namespace

Z80::Z80(Ctx &ctx) : TargetInfo(ctx) {
  // HALT.
  trapInstr = {0x76, 0x76, 0x76, 0x76};
  defaultImageBase = 0x100;
  defaultMaxPageSize = 1;
  defaultCommonPageSize = 1;
}

RelExpr Z80::getRelExpr(RelType type, const Symbol &s,
                        const uint8_t *loc) const {
  switch (type) {
  case R_Z80_NONE:
    return R_NONE;
  case R_Z80_8_PCREL:
    return R_PC;
  default:
    return R_ABS;
  }
}

void Z80::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_Z80_8:
    checkIntUInt(ctx, loc, val, 8, rel);
    *loc = val;
    break;
  case R_Z80_8_DIS:
  case R_Z80_8_PCREL:
    checkInt(ctx, loc, val, 8, rel);
    *loc = val;
    break;
  case R_Z80_16:
    checkIntUInt(ctx, loc, val, 16, rel);
    write16le(loc, val);
    break;
  case R_Z80_24:
    checkIntUInt(ctx, loc, val, 24, rel);
    loc[0] = val;
    loc[1] = val >> 8;
    loc[2] = val >> 16;
    break;
  case R_Z80_32:
    write32le(loc, val);
    break;
  case R_Z80_BYTE0:
    *loc = val;
    break;
  case R_Z80_BYTE1:
    *loc = val >> 8;
    break;
  case R_Z80_BYTE2:
    *loc = val >> 16;
    break;
  case R_Z80_BYTE3:
    *loc = val >> 24;
    break;
  case R_Z80_WORD0:
    write16le(loc, val);
    break;
  case R_Z80_WORD1:
    write16le(loc, val >> 16);
    break;
  case R_Z80_16_BE:
    checkIntUInt(ctx, loc, val, 16, rel);
    write16be(loc, val);
    break;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unrecognized relocation " << rel.type;
  }
}

void elf::setZ80TargetInfo(Ctx &ctx) { ctx.target.reset(new Z80(ctx)); }
