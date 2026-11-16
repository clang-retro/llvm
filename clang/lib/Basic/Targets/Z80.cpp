//===--- Z80.cpp - Implement Z80 and 8080 target feature support ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Zilog Z80 and Intel 8080 TargetInfo.
//
//===----------------------------------------------------------------------===//

#include "Z80.h"
#include "clang/Basic/MacroBuilder.h"
#include "llvm/ADT/StringSwitch.h"

using namespace clang;
using namespace clang::targets;

Z80TargetInfo::Z80TargetInfo(const llvm::Triple &Triple, const TargetOptions &)
    : TargetInfo(Triple) {
  CPU = Triple.getArch() == llvm::Triple::z80 ? "z80" : "i8080";
  TLSSupported = false;
  BigEndian = false;
  NoAsmVariants = true;
  PointerWidth = 16;
  PointerAlign = 8;
  BoolWidth = BoolAlign = 8;
  ShortWidth = 16;
  ShortAlign = 8;
  HalfWidth = 16;
  HalfAlign = 8;
  BFloat16Width = 16;
  BFloat16Align = 8;
  Float128Align = 8;
  Ibm128Align = 8;
  LargeArrayAlign = 8;
  IntWidth = 16;
  IntAlign = 8;
  LongWidth = 32;
  LongAlign = 8;
  LongLongWidth = 64;
  LongLongAlign = 8;
  ShortAccumWidth = 16;
  ShortAccumAlign = 8;
  AccumWidth = 32;
  AccumAlign = 8;
  LongAccumWidth = 64;
  LongAccumAlign = 8;
  FractWidth = FractAlign = 8;
  LongFractWidth = 16;
  LongFractAlign = 8;
  AccumScale = 15;
  LongAccumScale = 31;
  FloatWidth = 32;
  FloatAlign = 8;
  // double and long double are IEEE single, as in most compilers for 8-bit
  // processors (and avr-gcc): soft double precision is too big and slow
  // for a 64 KB machine. -mdouble=64 makes them IEEE double, which needs a
  // C library built the same way.
  DoubleWidth = 32;
  DoubleAlign = 8;
  DoubleFormat = &llvm::APFloat::IEEEsingle();
  LongDoubleWidth = 32;
  LongDoubleAlign = 8;
  LongDoubleFormat = &llvm::APFloat::IEEEsingle();
  SuitableAlign = 8;
  DefaultAlignForAttributeAligned = 8;
  MinGlobalAlign = 8;
  MaxAtomicPromoteWidth = MaxAtomicInlineWidth = 0;
  SizeType = UnsignedInt;
  PtrDiffType = SignedInt;
  IntPtrType = SignedInt;
  IntMaxType = SignedLongLong;
  Char16Type = UnsignedInt;
  Char32Type = UnsignedLong;
  WCharType = SignedInt;
  WIntType = SignedInt;
  SigAtomicType = SignedChar;
  ProcessIDType = SignedInt;
  resetDataLayout();
}

static const char *const GCCRegNames[] = {"a",  "b",  "c",  "d",  "e",
                                          "h",  "l",  "bc", "de", "hl",
                                          "ix", "iy", "sp", "af"};

ArrayRef<const char *> Z80TargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

bool Z80TargetInfo::hasFeature(StringRef Feature) const {
  return Feature == "z80" ? CPU == "z80" : Feature == "i8080";
}

bool Z80TargetInfo::isValidCPUName(StringRef Name) const {
  // A Z80 runs 8080 code, but not the other way round.
  if (getTriple().getArch() == llvm::Triple::i8080)
    return Name == "i8080" || Name == "i8085";
  return Name == "z80" || Name == "i8080" || Name == "i8085";
}

void Z80TargetInfo::fillValidCPUList(SmallVectorImpl<StringRef> &Values) const {
  Values.push_back("i8080");
  Values.push_back("i8085");
  if (getTriple().getArch() == llvm::Triple::z80)
    Values.push_back("z80");
}

bool Z80TargetInfo::setCPU(StringRef Name) {
  if (!isValidCPUName(Name))
    return false;
  CPU = Name.str();
  return true;
}

bool Z80TargetInfo::validateAsmConstraint(
    const char *&Name, TargetInfo::ConstraintInfo &Info) const {
  switch (*Name) {
  case 'a': // A
    Info.setAllowsRegister();
    return true;
  case 'I': // An 8-bit constant.
    Info.setRequiresImmediate(-128, 255);
    return true;
  default:
    return false;
  }
}

void Z80TargetInfo::getTargetDefines(const LangOptions &Opts,
                                     MacroBuilder &Builder) const {
  // The 8080 macros hold for the Z80 too: it runs 8080 code.
  Builder.defineMacro("__i8080__");
  Builder.defineMacro("__8080__");
  if (CPU == "i8085")
    Builder.defineMacro("__i8085__");
  if (CPU == "z80") {
    Builder.defineMacro("__z80__");
    Builder.defineMacro("__Z80__");
    Builder.defineMacro("__z80");
  }
  // CP/M-80. __CPM__ is for any CP/M, __CPM80__ for the 8080 one.
  if (getTriple().isOSCPM()) {
    Builder.defineMacro("__CPM__");
    Builder.defineMacro("__CPM80__");
  }
}
