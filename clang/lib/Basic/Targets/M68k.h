//===--- M68k.h - Declare M68k target feature support -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares M68k TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_M68K_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_M68K_H

#include "OSTargets.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"
#include <optional>

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY M68kTargetInfo : public TargetInfo {
  static const char *const GCCRegNames[];
  static const TargetInfo::GCCRegAlias GCCRegAliases[];

  enum CPUKind {
    CK_Unknown,
    CK_68000,
    CK_68010,
    CK_68020,
    CK_68030,
    CK_68040,
    CK_68060
  } CPU = CK_Unknown;

  const TargetOptions &TargetOpts;

public:
  M68kTargetInfo(const llvm::Triple &Triple, const TargetOptions &);

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;
  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override;
  bool hasFeature(StringRef Feature) const override;
  ArrayRef<const char *> getGCCRegNames() const override;
  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override;
  std::string convertConstraint(const char *&Constraint) const override;
  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &info) const override;
  std::optional<std::string> handleAsmEscapedChar(char EscChar) const override;
  std::string_view getClobbers() const override;
  BuiltinVaListKind getBuiltinVaListKind() const override;
  bool setCPU(StringRef Name) override;
  void fillValidCPUList(SmallVectorImpl<StringRef> &Values) const override;
  CallingConvCheckResult checkCallingConvention(CallingConv CC) const override;

  /// Give the DWARF numbers of the registers that hold the exception
  /// object (D0) and the type selector (D1) on entry to a landing pad.
  int getEHDataRegisterNumber(unsigned RegNo) const override {
    if (RegNo == 0)
      return 0;
    if (RegNo == 1)
      return 1;
    return -1;
  }

  std::pair<unsigned, unsigned> hardwareInterferenceSizes() const override {
    return std::make_pair(32, 32);
  }
};

} // namespace targets
} // namespace clang

#endif
