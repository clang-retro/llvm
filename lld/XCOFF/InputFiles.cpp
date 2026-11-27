//===- InputFiles.cpp -----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "Config.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Memory.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::XCOFF;

namespace lld::xcoff {
std::string toString(const ObjFile *f) {
  if (!f)
    return "<internal>";
  return f->getName();
}

ObjFile::ObjFile(MemoryBufferRef mb, StringRef archiveName)
    : mb(mb), archiveName(archiveName.str()) {}

std::string ObjFile::getName() const {
  if (archiveName.empty())
    return mb.getBufferIdentifier().str();
  return (archiveName + "(" + mb.getBufferIdentifier() + ")").str();
}

// Decide where a csect goes from its section and storage mapping class.
static OutKind classify(uint16_t styp, uint8_t smc, uint8_t symType,
                        const ObjFile *f, StringRef name) {
  if (symType == XTY_CM)
    return OutKind::Bss;
  switch (smc) {
  case XMC_TC0:
  case XMC_TC:
  case XMC_TD:
  case XMC_TE:
    return OutKind::Toc;
  case XMC_TL:
  case XMC_UL:
    error(toString(f) + ": thread-local csect " + name + " is not supported");
    return OutKind::Discard;
  default:
    break;
  }
  if (styp & STYP_TEXT)
    return OutKind::Text;
  if (styp & STYP_DATA)
    return OutKind::Data;
  if (styp & STYP_BSS)
    return OutKind::Bss;
  return OutKind::Discard;
}

void ObjFile::parse() {
  Expected<std::unique_ptr<ObjectFile>> o = ObjectFile::createObjectFile(mb);
  if (!o) {
    error(toString(this) + ": " + toString(o.takeError()));
    return;
  }
  if (!isa<XCOFFObjectFile>(o->get())) {
    error(toString(this) + ": not an XCOFF object");
    return;
  }
  obj.reset(cast<XCOFFObjectFile>(o->release()));
  if (obj->is64Bit()) {
    error(toString(this) + ": 64-bit XCOFF objects are not supported yet");
    return;
  }

  ArrayRef<XCOFFSectionHeader32> sections = obj->sections32();
  const uint8_t *base = reinterpret_cast<const uint8_t *>(mb.getBufferStart());
  // Csects of each section, sorted by address, to attach relocations.
  std::vector<std::vector<InputCsect *>> bySection(sections.size() + 1);

  uint32_t nsyms = obj->getNumberOfSymbolTableEntries();
  syms.resize(nsyms);

  for (uint32_t i = 0; i < nsyms;) {
    DataRefImpl d;
    d.p = obj->getSymbolEntryAddressByIndex(i);
    XCOFFSymbolRef sym(d, obj.get());
    uint32_t index = i;
    i += 1 + sym.getNumberOfAuxEntries();

    XCOFF::StorageClass sc = sym.getStorageClass();
    int16_t secNum = sym.getSectionNumber();
    uint32_t value = sym.getValue32();
    SymRef &ref = syms[index];
    ref.value = value;

    bool external = sc == C_EXT || sc == C_WEAKEXT;
    if (!external && sc != C_HIDEXT) {
      // C_FILE, C_STAT, debug symbols. A C_STAT symbol may still be the
      // target of a relocation: resolve it through its section.
      if (sc == C_STAT && secNum > 0 && (size_t)secNum <= sections.size()) {
        for (InputCsect *c : bySection[secNum])
          if (value >= c->oldAddr && value < c->oldAddr + std::max(1u, c->size))
            ref.csect = c;
        ref.valid = ref.csect != nullptr;
      }
      continue;
    }

    Expected<StringRef> nameOr = sym.getName();
    if (!nameOr) {
      error(toString(this) + ": " + toString(nameOr.takeError()));
      return;
    }
    StringRef name = *nameOr;

    Expected<XCOFFCsectAuxRef> auxOr = sym.getXCOFFCsectAuxRef();
    if (!auxOr) {
      error(toString(this) + ": " + toString(auxOr.takeError()));
      return;
    }
    XCOFFCsectAuxRef aux = *auxOr;
    uint8_t symType = aux.getSymbolType();
    uint8_t smc = aux.getStorageMappingClass();

    InputCsect *csect = nullptr;
    uint32_t offset = 0;
    if (secNum == N_ABS) {
      ref.absolute = true;
    } else if (secNum == N_DEBUG) {
      continue;
    } else if (symType == XTY_SD || symType == XTY_CM) {
      if (secNum <= 0 || (size_t)secNum > sections.size()) {
        error(toString(this) + ": csect " + name + " has a bad section number");
        return;
      }
      const XCOFFSectionHeader32 &sec = sections[secNum - 1];
      uint16_t styp = sec.getSectionType();
      csect = make<InputCsect>();
      csect->file = this;
      csect->name = name;
      csect->smc = smc;
      csect->symType = symType;
      csect->alignLog2 = aux.getAlignmentLog2();
      csect->oldAddr = value;
      csect->size = aux.getSectionOrLength32();
      csect->out = classify(styp, smc, symType, this, name);
      if (symType == XTY_SD && !(styp & STYP_BSS) && csect->size) {
        uint64_t start = (uint64_t)value - sec.VirtualAddress;
        if (start + csect->size > sec.SectionSize ||
            sec.FileOffsetToRawData + start + csect->size >
                mb.getBufferSize()) {
          error(toString(this) + ": csect " + name + " is out of bounds");
          return;
        }
        csect->data = ArrayRef<uint8_t>(base + sec.FileOffsetToRawData + start,
                                        csect->size);
      }
      csects.push_back(csect);
      bySection[secNum].push_back(csect);
    } else if (symType == XTY_LD) {
      uint32_t containing = aux.getSectionOrLength32();
      if (containing >= nsyms || !syms[containing].csect) {
        error(toString(this) + ": label " + name + " has no containing csect");
        return;
      }
      csect = syms[containing].csect;
      offset = value - csect->oldAddr;
    } else if (symType != XTY_ER) {
      error(toString(this) + ": symbol " + name + " has unknown type");
      return;
    }

    ref.csect = csect;
    ref.valid = true;
    if (!external)
      continue;

    // Register the external symbol.
    Symbol *s = symtabInsert(name);
    ref.sym = s;
    bool weak = sc == C_WEAKEXT;
    if (symType == XTY_ER) {
      if (!weak)
        s->referencedStrongly = true;
      if (s->kind == Symbol::Undefined && !s->file)
        s->file = this;
      if (s->kind == Symbol::Lazy) {
        // Fetching happens in the driver once this file is done.
        continue;
      }
      continue;
    }

    auto define = [&]() {
      if (secNum == N_ABS) {
        s->kind = Symbol::Absolute;
        s->absValue = value;
        s->csect = nullptr;
      } else {
        s->kind = Symbol::Defined;
        s->csect = csect;
        s->offset = offset;
      }
      s->weak = weak;
      s->file = this;
      s->lazy.reset();
    };

    switch (s->kind) {
    case Symbol::Undefined:
    case Symbol::Lazy:
    case Symbol::Imported:
      define();
      break;
    case Symbol::Defined:
    case Symbol::Absolute: {
      bool oldCommon = s->csect && s->csect->symType == XTY_CM;
      bool newCommon = symType == XTY_CM;
      if (oldCommon && newCommon) {
        if (csect->size > s->csect->size) {
          s->csect->live = false;
          define();
        } else {
          csect->live = false;
        }
      } else if (oldCommon && !newCommon) {
        s->csect->live = false;
        define();
      } else if (newCommon) {
        csect->live = false;
      } else if (s->weak && !weak) {
        define();
      } else if (!weak && !s->weak) {
        error("duplicate symbol: " + name + "\n>>> defined in " +
              toString(s->file) + "\n>>> defined in " + toString(this));
      }
      // Otherwise keep the first definition.
      break;
    }
    }
  }

  // Attach relocations to the csects that contain them.
  for (size_t si = 0; si < sections.size(); ++si) {
    const XCOFFSectionHeader32 &sec = sections[si];
    // Debug sections (DWARF, .debug, .except) are not linked.
    if (sec.NumberOfRelocations == 0 ||
        !(sec.Flags & (XCOFF::STYP_TEXT | XCOFF::STYP_DATA)))
      continue;
    std::vector<InputCsect *> &list = bySection[si + 1];
    llvm::stable_sort(list, [](const InputCsect *a, const InputCsect *b) {
      return a->oldAddr < b->oldAddr;
    });
    auto relsOr =
        obj->relocations<XCOFFSectionHeader32, XCOFFRelocation32>(sec);
    if (!relsOr) {
      error(toString(this) + ": " + toString(relsOr.takeError()));
      return;
    }
    for (const XCOFFRelocation32 &r : *relsOr) {
      uint32_t va = r.VirtualAddress;
      auto it =
          llvm::upper_bound(list, va, [](uint32_t v, const InputCsect *c) {
            return v < c->oldAddr;
          });
      if (it == list.begin()) {
        error(toString(this) + ": relocation at 0x" + utohexstr(va) +
              " is outside every csect");
        continue;
      }
      InputCsect *c = *(it - 1);
      if (va >= c->oldAddr + c->size) {
        error(toString(this) + ": relocation at 0x" + utohexstr(va) +
              " is outside csect " + c->name);
        continue;
      }
      if (r.SymbolIndex >= nsyms || !syms[r.SymbolIndex].valid) {
        error(toString(this) + ": relocation at 0x" + utohexstr(va) +
              " has a bad symbol index");
        continue;
      }
      c->relocs.push_back(
          {va, (uint32_t)r.SymbolIndex, r.Info, (uint8_t)r.Type});
    }
  }
}
} // namespace lld::xcoff
