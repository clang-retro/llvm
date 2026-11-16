//===- Z80.cpp ------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// Z80 and 8080 ABI Implementation
//
// Scalars of up to 32 bits go in HL and DE, the rest on the stack; the back
// end does that. A structure or union argument is copied by the caller and
// passed by address, and one that is returned is written through a pointer
// the caller passes first.
//===----------------------------------------------------------------------===//

namespace {

class Z80ABIInfo : public DefaultABIInfo {
public:
  Z80ABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  ABIArgInfo classifyArgumentType(QualType Ty) const {
    Ty = useFirstFieldIfTransparentUnion(Ty);
    if (isAggregateTypeForABI(Ty) || Ty->isAnyComplexType())
      return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                     /*ByVal=*/false);
    return DefaultABIInfo::classifyArgumentType(Ty);
  }

  ABIArgInfo classifyReturnType(QualType RetTy) const {
    if (RetTy->isAnyComplexType())
      return getNaturalAlignIndirect(RetTy,
                                     getDataLayout().getAllocaAddrSpace());
    return DefaultABIInfo::classifyReturnType(RetTy);
  }

  void computeInfo(CGFunctionInfo &FI) const override {
    if (!getCXXABI().classifyReturnType(FI))
      FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
    for (auto &I : FI.arguments())
      I.info = classifyArgumentType(I.type);
  }

  RValue EmitVAArg(CodeGenFunction &CGF, Address VAListAddr, QualType Ty,
                   AggValueSlot Slot) const override {
    return CGF.EmitLoadOfAnyValue(
        CGF.MakeAddrLValue(
            EmitVAArgInstr(CGF, VAListAddr, Ty, classifyArgumentType(Ty)), Ty),
        Slot);
  }
};

class Z80TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  Z80TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<Z80ABIInfo>(CGT)) {}
};

} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createZ80TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<Z80TargetCodeGenInfo>(CGM.getTypes());
}
