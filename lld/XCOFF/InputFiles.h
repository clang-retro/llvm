//===- InputFiles.h ---------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The XCOFF linker works on csects, the unit of relocation and placement in
// XCOFF.  Every csect of every input object becomes an InputCsect; the
// writer places them into the output .text, .data (with the TOC at its end)
// and .bss sections and applies their relocations.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_XCOFF_INPUT_FILES_H
#define LLD_XCOFF_INPUT_FILES_H

#include "lld/Common/LLVM.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/XCOFF.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/XCOFFObjectFile.h"
#include "llvm/Support/MemoryBufferRef.h"
#include <memory>
#include <string>
#include <vector>

namespace lld::xcoff {
class ObjFile;
struct Symbol;

// Where a csect ends up in the output.
enum class OutKind : uint8_t { Discard, Text, Data, Toc, Bss };

struct Reloc {
  uint32_t vaddr;  // r_vaddr: address of the field in the input object
  uint32_t symIdx; // r_symndx: input symbol table index
  uint8_t info;    // r_rsize: sign bit, fixup bit, field length minus one
  uint8_t type;    // r_rtype

  unsigned bitLength() const { return (info & 0x3f) + 1; }
};

struct InputCsect {
  ObjFile *file = nullptr; // null for csects the linker makes
  llvm::StringRef name;
  uint8_t smc = 0;     // storage mapping class (XMC_*)
  uint8_t symType = 0; // XTY_SD or XTY_CM
  uint8_t alignLog2 = 0;
  uint32_t oldAddr = 0; // address in the input object
  uint32_t size = 0;
  llvm::ArrayRef<uint8_t> data; // empty means zero filled
  std::vector<Reloc> relocs;
  OutKind out = OutKind::Discard;
  bool live = true;
  uint32_t outAddr = 0;

  bool isTOCAnchor() const { return smc == llvm::XCOFF::XMC_TC0; }
};

// What an input symbol table index refers to.
struct SymRef {
  Symbol *sym = nullptr;       // set for external symbols
  InputCsect *csect = nullptr; // set for local csect and label symbols
  uint32_t value = 0;          // n_value in the input object
  bool absolute = false;       // N_ABS local symbol
  bool valid = false;
};

class ObjFile {
public:
  ObjFile(llvm::MemoryBufferRef mb, llvm::StringRef archiveName);
  void parse();

  std::string getName() const;

  llvm::MemoryBufferRef mb;
  std::string archiveName;
  std::unique_ptr<llvm::object::XCOFFObjectFile> obj;
  std::vector<InputCsect *> csects;
  std::vector<SymRef> syms; // indexed by raw symbol table index
};

std::string toString(const ObjFile *f);
} // namespace lld::xcoff

#endif
