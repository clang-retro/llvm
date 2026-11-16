//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80SelectionDAGInfo.h"

#define GET_SDNODE_DESC
#include "Z80GenSDNodeInfo.inc"

using namespace llvm;

Z80SelectionDAGInfo::Z80SelectionDAGInfo()
    : SelectionDAGGenTargetInfo(Z80GenSDNodeInfo) {}

Z80SelectionDAGInfo::~Z80SelectionDAGInfo() = default;
