//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80MCAsmInfo.h"
using namespace llvm;

void Z80MCAsmInfo::anchor() {}

Z80MCAsmInfo::Z80MCAsmInfo(const Triple &TT, const MCTargetOptions &Options)
    : MCAsmInfoELF(TT, Options) {
  CodePointerSize = 2;
  CalleeSaveStackSlotSize = 2;
  MaxInstLength = 4;
  CommentString = ";";
  SeparatorString = "{";
  AlignmentIsInBytes = false;
  SupportsDebugInformation = true;
}
