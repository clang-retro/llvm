//===- Config.h -------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLD_XCOFF_CONFIG_H
#define LLD_XCOFF_CONFIG_H

#include "Symbols.h"
#include "llvm/ADT/StringMap.h"
#include <string>
#include <vector>

namespace lld::xcoff {
struct Config {
  std::string outputFile = "a.out";
  std::string entry = "__start";
  bool noEntry = false;
  uint32_t textBase = 0x10000000;
  uint32_t dataBase = 0x20000000;
  std::string libPath; // first loader import ID
  bool libPathSet = false;
  std::vector<std::string> searchPaths;
  bool strip = false;
  bool verbose = false;
  uint32_t maxStack = 0;
  uint32_t maxData = 0;
};

struct Ctx {
  Config arg;
  llvm::StringMap<Symbol *> symtab;
  std::vector<ObjFile *> objects;
  std::vector<std::unique_ptr<llvm::object::Archive>> archives;
  std::vector<std::unique_ptr<llvm::MemoryBuffer>> buffers;
  std::vector<ImportFile *> importFiles;
  llvm::StringMap<ImportEntry> imports;
  std::vector<InputCsect *> synthetic; // glink code and its TOC entries

  void reset() { *this = Ctx(); }
};

extern Ctx ctx;

Symbol *symtabFind(llvm::StringRef name);
Symbol *symtabInsert(llvm::StringRef name);
} // namespace lld::xcoff

#endif
