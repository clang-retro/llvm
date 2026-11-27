//===- Symbols.h ------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLD_XCOFF_SYMBOLS_H
#define LLD_XCOFF_SYMBOLS_H

#include "InputFiles.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Object/Archive.h"
#include <optional>

namespace lld::xcoff {
// An entry of an import file: a symbol that the system loader resolves from
// another module at exec time, such as a kernel service exported by /unix.
struct ImportFile {
  std::string path; // import file ID: path, base name, archive member
  std::string base;
  std::string member;
  unsigned id = 0; // index in the loader import table (1-based)
};

struct ImportEntry {
  ImportFile *file;
  uint8_t smc; // XMC_SV, XMC_SV3264, XMC_DS or XMC_UA
};

struct Symbol {
  enum Kind : uint8_t { Undefined, Defined, Lazy, Imported, Absolute };

  llvm::StringRef name;
  Kind kind = Undefined;
  bool weak = false; // defined weak, or only referenced weakly
  bool referencedStrongly = false;

  // Defined: the csect and offset from its start.
  InputCsect *csect = nullptr;
  uint32_t offset = 0;
  ObjFile *file = nullptr;

  // Absolute.
  uint32_t absValue = 0;

  // Lazy.
  std::optional<llvm::object::Archive::Symbol> lazy;

  // Imported.
  const ImportEntry *import = nullptr;
  int ldIndex = -1; // index among the loader section symbols

  // Global linkage code made for a call to an imported function, and the
  // TOC entry it loads the function descriptor from.
  InputCsect *glink = nullptr;
  InputCsect *tocEntry = nullptr;
};
} // namespace lld::xcoff

#endif
