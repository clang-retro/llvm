//===- Writer.cpp ---------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Lays out a 32-bit XCOFF executable:
//
//   file header, auxiliary header, section headers
//   .text    at textBase + file offset (code, read-only data, glink code)
//   .data    data, function descriptors, then the TOC
//   .bss
//   .loader  imports, and relocations for every address stored in .data
//
// The AIX loader relocates .data with the .loader relocations, binds
// imported symbols, and starts the program through the entry point's
// function descriptor.
//
//===----------------------------------------------------------------------===//

#include "Writer.h"
#include "Config.h"
#include "InputFiles.h"
#include "Symbols.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/BinaryFormat/XCOFF.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;
using namespace llvm::XCOFF;
using namespace llvm::support::endian;

namespace lld::xcoff {
namespace {
constexpr uint32_t headerSize =
    FileHeaderSize32 + AuxFileHeaderSize32 + 4 * SectionHeaderSize32;
constexpr uint16_t snText = 1, snData = 2, snBss = 3, snLoader = 4;

// lwz r2,20(r1): restores the TOC after a call through glink code.
constexpr uint32_t restoreTOC = 0x80410014;
constexpr uint32_t nopInsn = 0x60000000;

// Global linkage code, as the AIX binder makes it.  The first instruction
// gets the TOC offset of the function's descriptor address.
constexpr uint32_t glinkCode[9] = {
    0x81820000, // lwz   r12,0(r2)
    0x90410014, // stw   r2,20(r1)
    0x800c0000, // lwz   r0,0(r12)
    0x804c0004, // lwz   r2,4(r12)
    0x7c0903a6, // mtctr r0
    0x4e800420, // bctr
    0x00000000, // traceback table
    0x000c8000, //
    0x00000000, //
};

struct LoaderReloc {
  uint32_t vaddr;
  uint32_t symndx;
  uint16_t rtype;
  uint16_t secnm;
};

class Writer {
public:
  void run();

private:
  void layout();
  uint32_t csectAddr(const InputCsect *c) const;
  bool symbolAddr(const ObjFile *f, uint32_t idx, uint32_t &newAddr,
                  uint32_t &oldAddr, Symbol *&global, bool &isGL) const;
  void applyRelocs(InputCsect *c, uint8_t *buf);
  void writeLoader(uint8_t *buf);
  void writeHeaders(uint8_t *buf);
  uint8_t *at(uint32_t vaddr, const InputCsect *c);

  std::vector<InputCsect *> text, data, toc, bss;
  std::vector<Symbol *> ldSyms;
  std::vector<LoaderReloc> ldRelocs;

  uint32_t textOff = 0, textAddr = 0, textSize = 0, textAlign = 5;
  uint32_t dataOff = 0, dataAddr = 0, dataSize = 0, dataAlign = 3;
  uint32_t bssAddr = 0, bssSize = 0;
  uint32_t loaderOff = 0, loaderSize = 0;
  uint32_t tocStart = 0, tocEnd = 0, tocBase = 0;
  uint32_t fileSize = 0;
  uint8_t *outBuf = nullptr;

  // Loader section layout.
  std::string ldImpIds;
  std::string ldStrings;
};

static uint32_t place(std::vector<InputCsect *> &list, uint32_t addr) {
  for (InputCsect *c : list) {
    addr = alignTo(addr, uint64_t(1) << c->alignLog2);
    c->outAddr = addr;
    addr += c->size;
  }
  return addr;
}

void Writer::layout() {
  std::vector<InputCsect *> anchors, tocItems, tocEndItems;
  auto add = [&](InputCsect *c) {
    if (!c->live)
      return;
    switch (c->out) {
    case OutKind::Discard:
      break;
    case OutKind::Text:
      text.push_back(c);
      textAlign = std::max<uint32_t>(textAlign, c->alignLog2);
      break;
    case OutKind::Data:
      data.push_back(c);
      dataAlign = std::max<uint32_t>(dataAlign, c->alignLog2);
      break;
    case OutKind::Toc:
      if (c->isTOCAnchor())
        anchors.push_back(c);
      else if (c->smc == XMC_TE)
        tocEndItems.push_back(c);
      else
        tocItems.push_back(c);
      dataAlign = std::max<uint32_t>(dataAlign, c->alignLog2);
      break;
    case OutKind::Bss:
      bss.push_back(c);
      break;
    }
  };
  for (ObjFile *f : ctx.objects)
    for (InputCsect *c : f->csects)
      add(c);
  for (InputCsect *c : ctx.synthetic)
    add(c);
  llvm::append_range(toc, anchors);
  llvm::append_range(toc, tocItems);
  llvm::append_range(toc, tocEndItems);

  textOff = alignTo(headerSize, uint64_t(1) << textAlign);
  textAddr = ctx.arg.textBase + textOff;
  textSize = place(text, textAddr) - textAddr;

  dataOff = alignTo(textOff + textSize, uint64_t(1) << dataAlign);
  // Keep the data address congruent to its file offset modulo a page, as
  // the AIX binder does.
  dataAddr = ctx.arg.dataBase + (dataOff & 0xfff);
  uint32_t end = place(data, dataAddr);
  end = alignTo(end, 4);
  tocStart = end;
  end = place(toc, end);
  tocEnd = end;
  dataSize = end - dataAddr;

  if (tocEnd - tocStart > 0x10000)
    error("TOC overflow: " + Twine(tocEnd - tocStart) +
          " bytes is more than 64 KiB");
  tocBase = tocEnd - tocStart < 0x8000 ? tocStart : tocStart + 0x8000;

  uint32_t bssAlign = 3;
  for (InputCsect *c : bss)
    bssAlign = std::max<uint32_t>(bssAlign, c->alignLog2);
  bssAddr = alignTo(dataAddr + dataSize, uint64_t(1) << bssAlign);
  bssSize = place(bss, bssAddr) - bssAddr;

  loaderOff = alignTo(dataOff + dataSize, 4);

  for (auto &e : ctx.symtab) {
    Symbol *s = e.second;
    switch (s->boundary) {
    case Symbol::NoBoundary:
      break;
    case Symbol::TextStart:
      s->absValue = textAddr;
      break;
    case Symbol::TextEnd:
      s->absValue = textAddr + textSize;
      break;
    case Symbol::DataStart:
      s->absValue = dataAddr;
      break;
    case Symbol::DataEnd:
      s->absValue = dataAddr + dataSize;
      break;
    case Symbol::BssEnd:
      s->absValue = bssAddr + bssSize;
      break;
    }
  }
}

uint32_t Writer::csectAddr(const InputCsect *c) const {
  if (c->isTOCAnchor())
    return tocBase;
  return c->outAddr;
}

// Resolves input symbol IDX of F. Returns false for an imported symbol,
// whose address the loader supplies.
bool Writer::symbolAddr(const ObjFile *f, uint32_t idx, uint32_t &newAddr,
                        uint32_t &oldAddr, Symbol *&global, bool &isGL) const {
  const SymRef &r = f->syms[idx];
  oldAddr = r.value;
  global = r.sym;
  isGL = false;
  if (Symbol *s = r.sym) {
    switch (s->kind) {
    case Symbol::Defined:
      newAddr = csectAddr(s->csect) + s->offset;
      isGL = s->csect->smc == XMC_GL;
      return true;
    case Symbol::Absolute:
      newAddr = s->absValue;
      return true;
    case Symbol::Imported:
      newAddr = 0;
      return false;
    case Symbol::Undefined:
    case Symbol::Lazy:
      newAddr = 0; // weak undefined
      return true;
    }
  }
  if (r.absolute) {
    newAddr = r.value;
    return true;
  }
  newAddr = csectAddr(r.csect) + (r.value - r.csect->oldAddr);
  isGL = r.csect->smc == XMC_GL;
  return true;
}

static uint16_t outSection(const InputCsect *c) {
  switch (c->out) {
  case OutKind::Text:
    return snText;
  case OutKind::Data:
  case OutKind::Toc:
    return snData;
  case OutKind::Bss:
    return snBss;
  default:
    return 0;
  }
}

void Writer::applyRelocs(InputCsect *c, uint8_t *buf) {
  ObjFile *f = c->file;
  for (const Reloc &r : c->relocs) {
    uint32_t off = r.vaddr - c->oldAddr;
    uint32_t p = c->outAddr + off;
    uint8_t *loc = buf + off;
    unsigned bits = r.bitLength();
    auto fail = [&](const Twine &msg) {
      error(toString(f) + ":(" + c->name + "+0x" + utohexstr(off) +
            "): " + msg);
    };
    if (off + (bits > 16 ? 4 : 2) > c->size) {
      fail("relocation runs past the end of its csect");
      continue;
    }

    uint32_t s, sOld;
    Symbol *global;
    bool isGL;
    bool resolved = symbolAddr(f, r.symIdx, s, sOld, global, isGL);

    switch (r.type) {
    case R_REF:
      break;

    case R_POS:
    case R_RL:
    case R_RLA: {
      if (bits == 32) {
        uint32_t v = read32be(loc) - sOld + s;
        write32be(loc, v);
      } else if (bits == 16) {
        uint16_t v = read16be(loc) - sOld + s;
        write16be(loc, v);
      } else {
        fail("R_POS of " + Twine(bits) + " bits is not supported");
        continue;
      }
      // Every address stored in writable data gets a loader relocation, so
      // the loader can relocate .data and bind imports.
      bool writable = c->out == OutKind::Data || c->out == OutKind::Toc;
      bool absoluteTarget =
          resolved && ((global && (global->kind == Symbol::Absolute ||
                                   global->kind == Symbol::Undefined ||
                                   global->kind == Symbol::Lazy)) ||
                       (!global && f->syms[r.symIdx].absolute));
      if (!resolved && !writable) {
        fail("reference to imported " + global->name +
             " from read-only code or data");
        continue;
      }
      if (!writable || absoluteTarget)
        break;
      if (bits != 32) {
        fail("a 16-bit address in data cannot be relocated by the loader");
        continue;
      }
      LoaderReloc lr;
      lr.vaddr = p;
      lr.rtype = 0x1f00 | R_POS;
      lr.secnm = snData;
      if (!resolved) {
        if (global->ldIndex < 0) {
          global->ldIndex = ldSyms.size();
          ldSyms.push_back(global);
        }
        lr.symndx = 3 + global->ldIndex;
      } else {
        const InputCsect *target =
            global ? global->csect : f->syms[r.symIdx].csect;
        uint16_t sn = outSection(target);
        lr.symndx = sn == snText ? 0 : sn == snData ? 1 : 2;
      }
      ldRelocs.push_back(lr);
      break;
    }

    case R_NEG: {
      if (!resolved) {
        fail("R_NEG against imported " + global->name);
        continue;
      }
      if (bits != 32) {
        fail("R_NEG of " + Twine(bits) + " bits is not supported");
        continue;
      }
      write32be(loc, read32be(loc) + sOld - s);
      break;
    }

    case R_REL: {
      if (!resolved) {
        fail("pc-relative reference to imported " + global->name);
        continue;
      }
      if (bits != 32) {
        fail("R_REL of " + Twine(bits) + " bits is not supported");
        continue;
      }
      uint32_t pOld = r.vaddr;
      write32be(loc, read32be(loc) + (s - sOld) - (p - pOld));
      break;
    }

    case R_TOC:
    case R_TRL:
    case R_TRLA:
    case R_TCL:
    case R_GL:
    case R_TOCU:
    case R_TOCL: {
      if (!resolved) {
        fail("TOC-relative reference to imported " + global->name);
        continue;
      }
      int64_t v = (int64_t)s - (int64_t)tocBase;
      uint16_t field;
      if (r.type == R_TOCU) {
        field = (uint16_t)((v + 0x8000) >> 16);
      } else if (r.type == R_TOCL) {
        field = (uint16_t)v;
      } else {
        if (v < -0x8000 || v > 0x7fff) {
          fail("TOC offset " + Twine(v) + " does not fit in 16 bits");
          continue;
        }
        field = (uint16_t)v;
      }
      write16be(loc, field);
      break;
    }

    case R_BR:
    case R_RBR: {
      if (!resolved) {
        fail("branch to imported descriptor " + global->name +
             "; calls must go to the entry point ." + global->name);
        continue;
      }
      uint32_t insn = read32be(loc);
      // A call to an undefined weak function becomes a nop, as GNU ld does
      // on PowerPC; the caller tests the function's address first.
      if (global && global->kind == Symbol::Undefined) {
        write32be(loc, nopInsn);
        break;
      }
      if (insn & 2) {
        // An absolute branch.
        uint32_t target = (insn & 0x03fffffc) - sOld + s;
        if (target & 0xfc000000) {
          fail("absolute branch target 0x" + utohexstr(target) +
               " is out of range");
          continue;
        }
        write32be(loc, (insn & ~0x03fffffcu) | (target & 0x03fffffc));
        break;
      }
      int32_t oldDisp = SignExtend32<26>(insn & 0x03fffffc);
      int64_t addend = (int64_t)oldDisp - ((int64_t)sOld - (int64_t)r.vaddr);
      int64_t disp = (int64_t)s + addend - (int64_t)p;
      if (disp < -0x2000000 || disp >= 0x2000000) {
        fail("branch to " + (global ? global->name : StringRef("local")) +
             " is out of range");
        continue;
      }
      write32be(loc, (insn & ~0x03fffffcu) | ((uint32_t)disp & 0x03fffffc));
      // The instruction after a call through glink code restores the TOC.
      if (off + 8 <= c->size) {
        uint32_t next = read32be(loc + 4);
        if (isGL &&
            (next == nopInsn || next == 0x4def7b82 || next == 0x4ffffb82))
          write32be(loc + 4, restoreTOC);
        else if (!isGL && next == restoreTOC)
          write32be(loc + 4, nopInsn);
      }
      break;
    }

    case R_BA:
    case R_RBA: {
      if (!resolved) {
        fail("absolute branch to imported " + global->name);
        continue;
      }
      uint32_t insn = read32be(loc);
      uint32_t target = (insn & 0x03fffffc) - sOld + s;
      write32be(loc, (insn & ~0x03fffffcu) | (target & 0x03fffffc));
      break;
    }

    default:
      fail("unsupported relocation type 0x" + utohexstr(r.type));
      continue;
    }
  }
}

static void putName(uint8_t *p, StringRef name, std::string &strings,
                    uint32_t lenFieldSize) {
  if (name.size() <= NameSize) {
    memset(p, 0, NameSize);
    memcpy(p, name.data(), name.size());
    return;
  }
  write32be(p, 0);
  uint32_t off = strings.size() + lenFieldSize;
  write32be(p + 4, off);
  if (lenFieldSize == 2) {
    strings.push_back((char)(((name.size() + 1) >> 8) & 0xff));
    strings.push_back((char)((name.size() + 1) & 0xff));
  }
  strings += name;
  strings.push_back('\0');
}

void Writer::writeLoader(uint8_t *buf) {
  constexpr uint32_t hdrSize = 32, symSize = 24, relSize = 12;
  uint8_t *p = buf;
  uint32_t nsyms = ldSyms.size();
  uint32_t nrel = ldRelocs.size();
  uint32_t impOff = hdrSize + nsyms * symSize + nrel * relSize;
  uint32_t stOff = impOff + ldImpIds.size();

  write32be(p + 0, 1); // l_version
  write32be(p + 4, nsyms);
  write32be(p + 8, nrel);
  write32be(p + 12, ldImpIds.size());            // l_istlen
  write32be(p + 16, ctx.importFiles.size() + 1); // l_nimpid
  write32be(p + 20, impOff);
  write32be(p + 24, ldStrings.size());
  write32be(p + 28, ldStrings.empty() ? 0 : stOff);
  p += hdrSize;

  std::string unused;
  for (Symbol *s : ldSyms) {
    // Names were laid out in ldStrings already; recompute in the same order.
    putName(p, s->name, unused, 2);
    if (s->kind == Symbol::Imported) {
      write32be(p + 8, 0);    // l_value
      write16be(p + 12, 0);   // l_scnum
      p[14] = 0x40;           // L_IMPORT | XTY_ER
      p[15] = s->import->smc; // l_smclas
      write32be(p + 16, s->import->file->id);
    } else {
      bool isEntry = !ctx.arg.noEntry && s->name == ctx.arg.entry;
      bool isLabel = s->offset != 0 || s->csect->name != s->name;
      write32be(p + 8, csectAddr(s->csect) + s->offset);
      write16be(p + 12, outSection(s->csect));
      // L_ENTRY and L_EXPORT, with the symbol type.
      p[14] = (isEntry ? 0x20 : 0) | (s->exported ? 0x10 : 0) |
              (isLabel ? XTY_LD : s->csect->symType);
      p[15] = s->csect->smc;
      write32be(p + 16, 0);
    }
    write32be(p + 20, 0); // l_parm
    p += symSize;
  }
  for (const LoaderReloc &r : ldRelocs) {
    write32be(p + 0, r.vaddr);
    write32be(p + 4, r.symndx);
    write16be(p + 8, r.rtype);
    write16be(p + 10, r.secnm);
    p += relSize;
  }
  memcpy(p, ldImpIds.data(), ldImpIds.size());
  p += ldImpIds.size();
  memcpy(p, ldStrings.data(), ldStrings.size());
}

void Writer::writeHeaders(uint8_t *buf) {
  uint8_t *p = buf;
  // File header.
  write16be(p + 0, 0x01DF); // f_magic
  write16be(p + 2, 4);      // f_nscns
  write32be(p + 4, 0);      // f_timdat
  write32be(p + 8, 0);      // f_symptr
  write32be(p + 12, 0);     // f_nsyms
  write16be(p + 16, AuxFileHeaderSize32);
  // F_RELFLG | F_EXEC | F_LNNO | F_DYNLOAD
  write16be(p + 18, 0x0001 | 0x0002 | 0x0004 | 0x1000);
  p += FileHeaderSize32;

  // Auxiliary header.
  uint32_t entry = 0xffffffff;
  uint16_t snEntry = 0;
  if (!ctx.arg.noEntry) {
    Symbol *e = symtabFind(ctx.arg.entry);
    entry = csectAddr(e->csect) + e->offset;
    snEntry = outSection(e->csect);
  }
  write16be(p + 0, 0x010B); // o_mflag
  write16be(p + 2, 1);      // o_vstamp
  write32be(p + 4, textSize);
  write32be(p + 8, dataSize);
  write32be(p + 12, bssSize);
  write32be(p + 16, entry);
  write32be(p + 20, textAddr);
  write32be(p + 24, dataAddr);
  write32be(p + 28, tocBase);
  write16be(p + 32, snEntry);
  write16be(p + 34, snText);
  write16be(p + 36, snData);
  write16be(p + 38, snData); // o_sntoc
  write16be(p + 40, snLoader);
  write16be(p + 42, snBss);
  write16be(p + 44, textAlign);
  write16be(p + 46, dataAlign);
  p[48] = '1'; // o_modtype "1L"
  p[49] = 'L';
  p[50] = 0; // o_cpuflag
  p[51] = 3; // o_cputype: common PowerPC/POWER
  write32be(p + 52, ctx.arg.maxStack);
  write32be(p + 56, ctx.arg.maxData);
  write32be(p + 60, 0); // o_debugger
  // o_textpsize, o_datapsize, o_stackpsize, o_flags, o_sntdata, o_sntbss
  memset(p + 64, 0, 8);
  p += AuxFileHeaderSize32;

  auto section = [&](const char *name, uint32_t addr, uint32_t size,
                     uint32_t off, uint32_t flags) {
    memset(p, 0, SectionHeaderSize32);
    memcpy(p, name, strlen(name));
    write32be(p + 8, addr);  // s_paddr
    write32be(p + 12, addr); // s_vaddr
    write32be(p + 16, size);
    write32be(p + 20, off);
    write32be(p + 36, flags);
    p += SectionHeaderSize32;
  };
  section(".text", textAddr, textSize, textSize ? textOff : 0, STYP_TEXT);
  section(".data", dataAddr, dataSize, dataSize ? dataOff : 0, STYP_DATA);
  section(".bss", bssAddr, bssSize, 0, STYP_BSS);
  section(".loader", 0, loaderSize, loaderOff, STYP_LOADER);
}

uint8_t *Writer::at(uint32_t vaddr, const InputCsect *c) {
  if (c->out == OutKind::Text)
    return outBuf + textOff + (vaddr - textAddr);
  return outBuf + dataOff + (vaddr - dataAddr);
}

void Writer::run() {
  layout();
  if (errorCount())
    return;

  // The entry point is a loader symbol, as the AIX binder makes it.
  if (!ctx.arg.noEntry) {
    Symbol *e = symtabFind(ctx.arg.entry);
    e->ldIndex = ldSyms.size();
    ldSyms.push_back(e);
  }

  // Exported symbols, in name order.
  std::vector<Symbol *> exports;
  for (auto &e : ctx.symtab)
    if (e.second->exported && e.second->ldIndex < 0)
      exports.push_back(e.second);
  llvm::sort(exports, [](const Symbol *a, const Symbol *b) {
    return a->name < b->name;
  });
  for (Symbol *s : exports) {
    s->ldIndex = ldSyms.size();
    ldSyms.push_back(s);
  }

  // Relocate every placed csect into a scratch copy first, which collects
  // the loader relocations and loader symbols.
  std::vector<std::pair<InputCsect *, std::vector<uint8_t>>> contents;
  auto build = [&](std::vector<InputCsect *> &list) {
    for (InputCsect *c : list) {
      std::vector<uint8_t> bytes(c->size);
      if (!c->data.empty())
        memcpy(bytes.data(), c->data.data(), c->size);
      if (c->file)
        applyRelocs(c, bytes.data());
      contents.push_back({c, std::move(bytes)});
    }
  };
  build(text);
  build(data);
  build(toc);

  // Glink code and its TOC entries.
  for (auto &[c, bytes] : contents) {
    if (c->file)
      continue;
    Symbol *imp = nullptr;
    for (auto &e : ctx.symtab)
      if (e.second->kind == Symbol::Imported &&
          (e.second->glink == c || e.second->tocEntry == c))
        imp = e.second;
    if (!imp)
      continue;
    if (c == imp->glink) {
      int64_t off = (int64_t)imp->tocEntry->outAddr - tocBase;
      if (off < -0x8000 || off > 0x7fff) {
        error("TOC entry for " + imp->name + " is out of range");
        continue;
      }
      for (unsigned i = 0; i < 9; ++i)
        write32be(bytes.data() + 4 * i, glinkCode[i]);
      write32be(bytes.data(), glinkCode[0] | ((uint32_t)off & 0xffff));
    } else {
      if (imp->ldIndex < 0) {
        imp->ldIndex = ldSyms.size();
        ldSyms.push_back(imp);
      }
      ldRelocs.push_back(
          {c->outAddr, (uint32_t)(3 + imp->ldIndex), 0x1f00 | R_POS, snData});
    }
  }
  if (errorCount())
    return;

  llvm::stable_sort(ldRelocs, [](const LoaderReloc &a, const LoaderReloc &b) {
    return a.vaddr < b.vaddr;
  });

  // Import file IDs: the library search path first, then one entry for
  // each import file.
  ldImpIds = ctx.arg.libPath;
  ldImpIds.append(3, '\0');
  for (ImportFile *f : ctx.importFiles) {
    ldImpIds += f->path;
    ldImpIds.push_back('\0');
    ldImpIds += f->base;
    ldImpIds.push_back('\0');
    ldImpIds += f->member;
    ldImpIds.push_back('\0');
  }
  uint8_t scratch[8];
  for (Symbol *s : ldSyms)
    putName(scratch, s->name, ldStrings, 2);

  loaderSize = 32 + ldSyms.size() * 24 + ldRelocs.size() * 12 +
               ldImpIds.size() + ldStrings.size();
  fileSize = loaderOff + loaderSize;

  Expected<std::unique_ptr<FileOutputBuffer>> bufOr = FileOutputBuffer::create(
      ctx.arg.outputFile, fileSize, FileOutputBuffer::F_executable);
  if (!bufOr) {
    error("cannot open output file " + ctx.arg.outputFile + ": " +
          toString(bufOr.takeError()));
    return;
  }
  std::unique_ptr<FileOutputBuffer> buf = std::move(*bufOr);
  outBuf = buf->getBufferStart();
  memset(outBuf, 0, fileSize);

  writeHeaders(outBuf);
  for (auto &[c, bytes] : contents)
    memcpy(at(c->outAddr, c), bytes.data(), bytes.size());
  writeLoader(outBuf + loaderOff);

  if (Error e = buf->commit())
    error("failed to write " + ctx.arg.outputFile + ": " +
          toString(std::move(e)));
}
} // namespace

void writeResult() { Writer().run(); }
} // namespace lld::xcoff
