//===- Driver.cpp ---------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// An XCOFF linker for 32-bit AIX.  It takes the AIX binder's command line
// (-bI:, -bpT:, -bpD:, -blibpath:, -e, -o, -L, -l) and links static
// executables.  Symbols that are left undefined are resolved through import
// files, such as a list of the system calls that /unix exports, and are bound
// by the system loader when the program starts.
//
//===----------------------------------------------------------------------===//

#include "Config.h"
#include "InputFiles.h"
#include "Writer.h"
#include "lld/Common/Args.h"
#include "lld/Common/CommonLinkerContext.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Memory.h"
#include "lld/Common/Version.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/Magic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/StringSaver.h"

using namespace llvm;
using namespace llvm::object;

namespace lld::xcoff {
Ctx ctx;

Symbol *symtabFind(StringRef name) {
  auto it = ctx.symtab.find(name);
  return it == ctx.symtab.end() ? nullptr : it->second;
}

Symbol *symtabInsert(StringRef name) {
  auto [it, inserted] = ctx.symtab.try_emplace(name, nullptr);
  if (inserted) {
    it->second = make<Symbol>();
    it->second->name = it->first();
  }
  return it->second;
}

namespace {
struct Fetched {
  const Archive *archive;
  uint64_t offset;
  bool operator==(const Fetched &o) const {
    return archive == o.archive && offset == o.offset;
  }
};
} // namespace
} // namespace lld::xcoff

namespace llvm {
template <> struct DenseMapInfo<lld::xcoff::Fetched> {
  static lld::xcoff::Fetched getEmptyKey() { return {nullptr, ~0ULL}; }
  static lld::xcoff::Fetched getTombstoneKey() { return {nullptr, ~1ULL}; }
  static unsigned getHashValue(const lld::xcoff::Fetched &f) {
    return hash_combine(f.archive, f.offset);
  }
  static bool isEqual(const lld::xcoff::Fetched &a,
                      const lld::xcoff::Fetched &b) {
    return a == b;
  }
};
} // namespace llvm

namespace lld::xcoff {
namespace {
class LinkerDriver {
public:
  void linkerMain(ArrayRef<const char *> args);

private:
  bool parseArgs(ArrayRef<const char *> args);
  void addFile(StringRef path);
  void addLibrary(StringRef name);
  void addObject(MemoryBufferRef mb, StringRef archiveName);
  void addArchive(MemoryBufferRef mb);
  void addImportFile(MemoryBufferRef mb);
  void fetch(const Archive::Symbol &sym);
  void drain();
  void resolveImports();

  std::vector<std::string> inputs; // files and "-l" names, in order
  std::vector<std::string> exportFiles;
  DenseSet<Fetched> fetched;
  std::vector<Symbol *> pendingFetch;
};

std::optional<MemoryBufferRef> readFile(StringRef path) {
  auto mbOrErr = MemoryBuffer::getFile(path, /*IsText=*/false,
                                       /*RequiresNullTerminator=*/false);
  if (auto ec = mbOrErr.getError()) {
    error("cannot open " + path + ": " + ec.message());
    return std::nullopt;
  }
  MemoryBufferRef ref = (*mbOrErr)->getMemBufferRef();
  ctx.buffers.push_back(std::move(*mbOrErr));
  return ref;
}

bool parseNumber(StringRef s, uint32_t &out) {
  uint64_t v;
  StringRef t = s;
  unsigned radix = 10;
  if (t.consume_front_insensitive("0x"))
    radix = 16;
  if (t.getAsInteger(radix, v) || v > 0xffffffffULL)
    return false;
  out = (uint32_t)v;
  return true;
}

bool LinkerDriver::parseArgs(ArrayRef<const char *> argsArr) {
  BumpPtrAllocator alloc;
  StringSaver saver(alloc);
  SmallVector<const char *, 64> args(argsArr.begin() + 1, argsArr.end());
  cl::ExpansionContext ectx(saver.getAllocator(), cl::TokenizeGNUCommandLine);
  if (Error e = ectx.expandResponseFiles(args)) {
    error(toString(std::move(e)));
    return false;
  }
  std::vector<std::string> owned;
  for (const char *a : args)
    owned.push_back(a);

  Config &c = ctx.arg;
  for (size_t i = 0; i < owned.size(); ++i) {
    StringRef a = owned[i];
    auto next = [&](StringRef opt) -> std::optional<std::string> {
      if (i + 1 >= owned.size()) {
        error("missing argument to " + opt);
        return std::nullopt;
      }
      return owned[++i];
    };

    if (a == "--version" || a == "-version") {
      message(getLLDVersion() + " (XCOFF linker)");
      return false;
    }
    if (a == "--verbose" || a == "-V") {
      c.verbose = true;
      continue;
    }
    if (a == "-o") {
      if (auto v = next(a))
        c.outputFile = *v;
      continue;
    }
    if (a.starts_with("-o") && a.size() > 2) {
      c.outputFile = a.substr(2).str();
      continue;
    }
    if (a == "-L") {
      if (auto v = next(a))
        c.searchPaths.push_back(*v);
      continue;
    }
    if (a.starts_with("-L")) {
      c.searchPaths.push_back(a.substr(2).str());
      continue;
    }
    if (a == "-l") {
      if (auto v = next(a))
        inputs.push_back("-l" + *v);
      continue;
    }
    if (a.starts_with("-l")) {
      inputs.push_back(a.str());
      continue;
    }
    if (a == "-e") {
      if (auto v = next(a))
        c.entry = *v;
      continue;
    }
    if (a.starts_with("-e") && a.size() > 2) {
      c.entry = a.substr(2).str();
      continue;
    }
    if (a == "-s") {
      c.strip = true;
      continue;
    }
    if (a == "-b") {
      // The binder accepts "-b opt" as well as "-bopt".
      if (auto v = next(a))
        owned[i] = "-b" + *v, --i;
      continue;
    }
    if (a.starts_with("-b")) {
      StringRef b = a.substr(2);
      StringRef val;
      if (b.consume_front("I:") || b.consume_front("import:")) {
        inputs.push_back("-bI:" + b.str());
      } else if (b.consume_front("pT:")) {
        if (!parseNumber(b, c.textBase))
          error("bad address in " + a);
      } else if (b.consume_front("pD:")) {
        if (!parseNumber(b, c.dataBase))
          error("bad address in " + a);
      } else if (b.consume_front("libpath:")) {
        c.libPath = b.str();
        c.libPathSet = true;
      } else if (b == "nolibpath") {
        c.libPath.clear();
        c.libPathSet = false;
      } else if (b.consume_front("maxstack:")) {
        if (!parseNumber(b, c.maxStack))
          error("bad value in " + a);
      } else if (b.consume_front("maxdata:")) {
        if (!parseNumber(b, c.maxData))
          error("bad value in " + a);
      } else if (b == "noentry") {
        c.noEntry = true;
      } else if (b == "64") {
        error("64-bit XCOFF output is not supported yet");
      } else if (b.consume_front("E:") || b.consume_front("export:")) {
        exportFiles.push_back(b.str());
      } else if (b == "expall" || b == "expfull") {
        warn("ignoring " + a + ": list the symbols to export with -bE:");
      } else if (b.starts_with("M:") || b.starts_with("modtype:")) {
        if (b != "M:1L" && b != "modtype:1L")
          warn("ignoring " + a + ": only 1L modules are made");
      } else {
        // -b32, -bnso, -bso, -bcdtors, -bdbg, -bforceimprw, -bhalt, -bgc,
        // -bnoquiet and friends do not change what this linker makes.
        (void)val;
      }
      continue;
    }
    if (a.starts_with("-H") || a.starts_with("-T") || a.starts_with("-D") ||
        a == "-K" || a == "-z" || a == "-r" || a == "-static" ||
        a == "-bstatic") {
      if (a == "-r")
        error("relocatable output (-r) is not supported");
      continue;
    }
    if (a.starts_with("-")) {
      warn("ignoring unknown option " + a);
      continue;
    }
    inputs.push_back(a.str());
  }

  // The library path the loader searches for shared modules. The binder
  // would add the -L directories, but those are paths on the build machine.
  if (!c.libPathSet)
    c.libPath = "/usr/lib:/lib";
  return true;
}

void LinkerDriver::addObject(MemoryBufferRef mb, StringRef archiveName) {
  ObjFile *f = make<ObjFile>(mb, archiveName);
  ctx.objects.push_back(f);
  f->parse();
  for (const SymRef &r : f->syms)
    if (r.sym && r.sym->kind == Symbol::Lazy)
      pendingFetch.push_back(r.sym);
}

void LinkerDriver::fetch(const Archive::Symbol &sym) {
  Expected<Archive::Child> childOr = sym.getMember();
  if (!childOr) {
    error("cannot get archive member for " + sym.getName() + ": " +
          toString(childOr.takeError()));
    return;
  }
  const Archive *a = childOr->getParent();
  if (!fetched.insert({a, childOr->getChildOffset()}).second)
    return;
  Expected<MemoryBufferRef> mbOr = childOr->getMemoryBufferRef();
  if (!mbOr) {
    error(toString(mbOr.takeError()));
    return;
  }
  if (ctx.arg.verbose)
    message("fetching " + mbOr->getBufferIdentifier() + " for " +
            sym.getName());
  addObject(*mbOr, a->getMemoryBufferRef().getBufferIdentifier());
}

void LinkerDriver::drain() {
  while (!pendingFetch.empty()) {
    Symbol *s = pendingFetch.back();
    pendingFetch.pop_back();
    if (s->kind != Symbol::Lazy)
      continue;
    Archive::Symbol as = *s->lazy;
    s->lazy.reset();
    s->kind = Symbol::Undefined;
    fetch(as);
  }
}

void LinkerDriver::addArchive(MemoryBufferRef mb) {
  // Archive::create picks the AIX big archive reader for <bigaf> files.
  Expected<std::unique_ptr<Archive>> aOr = Archive::create(mb);
  if (!aOr) {
    error(mb.getBufferIdentifier() + ": " + toString(aOr.takeError()));
    return;
  }
  std::unique_ptr<Archive> a = std::move(*aOr);
  for (const Archive::Symbol &sym : a->symbols()) {
    Symbol *s = symtabInsert(sym.getName());
    if (s->kind == Symbol::Undefined) {
      // Only fetch for symbols something actually refers to.
      if (s->file || s->referencedStrongly) {
        s->kind = Symbol::Lazy;
        s->lazy = sym;
        pendingFetch.push_back(s);
      } else {
        s->kind = Symbol::Lazy;
        s->lazy = sym;
      }
    }
  }
  ctx.archives.push_back(std::move(a));
  drain();
}

// Import files list symbols that the system loader resolves at exec time.
//   #! /unix          following symbols come from the kernel
//   name [syscall]    a symbol, optionally marked as a system call
//   * comment, # comment
void LinkerDriver::addImportFile(MemoryBufferRef mb) {
  ImportFile *cur = nullptr;
  SmallVector<StringRef, 0> lines;
  mb.getBuffer().split(lines, '\n');
  for (StringRef line : lines) {
    line = line.trim();
    if (line.empty() || line[0] == '*')
      continue;
    if (line.starts_with("#!")) {
      StringRef p = line.substr(2).trim();
      if (p.empty()) {
        cur = nullptr;
        continue;
      }
      if (p.contains('(')) {
        error(mb.getBufferIdentifier() + ": #! (member) is not supported");
        continue;
      }
      cur = make<ImportFile>();
      StringRef baseName = sys::path::filename(p, sys::path::Style::posix);
      size_t dirLen = p.size() - baseName.size();
      if (dirLen == 0)
        cur->path = "";
      else if (dirLen == 1)
        cur->path = "/";
      else
        cur->path = p.substr(0, dirLen - 1).str();
      cur->base = baseName.str();
      // Share the ID of an identical import file.
      for (ImportFile *f : ctx.importFiles)
        if (f->path == cur->path && f->base == cur->base &&
            f->member == cur->member)
          cur = f;
      if (llvm::find(ctx.importFiles, cur) == ctx.importFiles.end()) {
        ctx.importFiles.push_back(cur);
        cur->id = ctx.importFiles.size();
      }
      continue;
    }
    if (line[0] == '#')
      continue;
    SmallVector<StringRef, 4> words;
    line.split(words, ' ', -1, false);
    StringRef name = words[0];
    // Lines that only change the 32/64-bit mode of the following symbols.
    if (name == "32" || name == "64" || name == "32_64" || name == "32-64")
      continue;
    if (!cur) {
      error(mb.getBufferIdentifier() + ": " + name +
            " is listed before any #! line naming the module it comes from");
      continue;
    }
    uint8_t smc = XCOFF::XMC_DS;
    for (StringRef w : ArrayRef(words).drop_front()) {
      if (w == "syscall" || w == "syscall32")
        smc = XCOFF::XMC_SV;
      else if (w == "syscall3264")
        smc = XCOFF::XMC_SV3264;
      else if (w == "syscall64")
        smc = XCOFF::XMC_SV64;
      else if (w == "data")
        smc = XCOFF::XMC_UA;
    }
    ctx.imports[name] = ImportEntry{cur, smc};
  }
}

void LinkerDriver::addFile(StringRef path) {
  std::optional<MemoryBufferRef> mb = readFile(path);
  if (!mb)
    return;
  StringRef buf = mb->getBuffer();
  if (buf.starts_with("#!")) {
    addImportFile(*mb);
    return;
  }
  switch (identify_magic(buf)) {
  case file_magic::archive:
    addArchive(*mb);
    return;
  case file_magic::xcoff_object_32:
    addObject(*mb, "");
    drain();
    return;
  case file_magic::xcoff_object_64:
    error(path + ": 64-bit XCOFF objects are not supported yet");
    return;
  default:
    error(path + ": unknown file type");
    return;
  }
}

void LinkerDriver::addLibrary(StringRef name) {
  for (const std::string &dir : ctx.arg.searchPaths) {
    SmallString<128> p(dir);
    sys::path::append(p, "lib" + name + ".a");
    if (sys::fs::exists(p)) {
      addFile(p);
      return;
    }
  }
  error("unable to find library -l" + name);
}

void LinkerDriver::resolveImports() {
  std::vector<Symbol *> undefs;
  for (auto &e : ctx.symtab)
    if (e.second->kind == Symbol::Undefined)
      undefs.push_back(e.second);
  // Report in a stable order.
  llvm::sort(undefs, [](const Symbol *a, const Symbol *b) {
    return a->name < b->name;
  });

  for (Symbol *s : undefs) {
    if (!s->file && !s->referencedStrongly)
      continue; // a name from an archive symbol table nobody used
    // The section boundaries the AIX binder defines.
    Symbol::Boundary b = StringSwitch<Symbol::Boundary>(s->name)
                             .Case("_text", Symbol::TextStart)
                             .Cases({"_etext", "etext"}, Symbol::TextEnd)
                             .Case("_data", Symbol::DataStart)
                             .Cases({"_edata", "edata"}, Symbol::DataEnd)
                             .Cases({"_end", "end"}, Symbol::BssEnd)
                             .Default(Symbol::NoBoundary);
    if (b != Symbol::NoBoundary) {
      s->kind = Symbol::Absolute;
      s->boundary = b;
      continue;
    }
    auto it = ctx.imports.find(s->name);
    if (it != ctx.imports.end()) {
      s->kind = Symbol::Imported;
      s->import = &it->second;
      continue;
    }
    if (s->name.starts_with(".")) {
      auto it2 = ctx.imports.find(s->name.drop_front());
      if (it2 != ctx.imports.end()) {
        // A direct call to an imported function: make global linkage code
        // that calls through the function's descriptor, as the AIX binder
        // does.
        Symbol *desc = symtabInsert(s->name.drop_front());
        if (desc->kind == Symbol::Undefined || desc->kind == Symbol::Lazy) {
          desc->kind = Symbol::Imported;
          desc->import = &it2->second;
          desc->lazy.reset();
        }
        if (desc->kind != Symbol::Imported) {
          error("cannot call imported " + desc->name +
                ": it is also defined in " + toString(desc->file));
          continue;
        }
        auto *glink = make<InputCsect>();
        glink->name = s->name;
        glink->smc = XCOFF::XMC_GL;
        glink->symType = XCOFF::XTY_SD;
        glink->alignLog2 = 2;
        glink->size = 36;
        glink->out = OutKind::Text;
        auto *toc = make<InputCsect>();
        toc->name = desc->name;
        toc->smc = XCOFF::XMC_TC;
        toc->symType = XCOFF::XTY_SD;
        toc->alignLog2 = 2;
        toc->size = 4;
        toc->out = OutKind::Toc;
        desc->glink = glink;
        desc->tocEntry = toc;
        ctx.synthetic.push_back(glink);
        ctx.synthetic.push_back(toc);
        s->kind = Symbol::Defined;
        s->csect = glink;
        s->offset = 0;
        continue;
      }
    }
    if (!s->referencedStrongly) {
      s->weak = true; // weak undefined, resolves to 0
      continue;
    }
    error("undefined symbol: " + s->name + "\n>>> referenced by " +
          toString(s->file));
  }
}

void LinkerDriver::linkerMain(ArrayRef<const char *> args) {
  if (!parseArgs(args))
    return;

  if (!ctx.arg.noEntry) {
    Symbol *e = symtabInsert(ctx.arg.entry);
    e->referencedStrongly = true;
  }

  for (const std::string &in : inputs) {
    StringRef s = in;
    if (s.consume_front("-bI:")) {
      if (std::optional<MemoryBufferRef> mb = readFile(s))
        addImportFile(*mb);
    } else if (s.consume_front("-l")) {
      addLibrary(s);
    } else {
      addFile(s);
    }
  }
  if (errorCount())
    return;

  resolveImports();
  if (errorCount())
    return;

  // Export files: one symbol per line, as for the AIX binder.
  for (const std::string &path : exportFiles) {
    std::optional<MemoryBufferRef> mb = readFile(path);
    if (!mb)
      continue;
    SmallVector<StringRef, 0> lines;
    mb->getBuffer().split(lines, '\n');
    for (StringRef line : lines) {
      line = line.trim();
      if (line.empty() || line[0] == '*' || line[0] == '#')
        continue;
      StringRef name = line.split(' ').first;
      Symbol *s = symtabFind(name);
      if (!s || s->kind != Symbol::Defined) {
        warn(path + ": cannot export " + name + ": it is not defined");
        continue;
      }
      s->exported = true;
    }
  }

  if (!ctx.arg.noEntry) {
    Symbol *e = symtabFind(ctx.arg.entry);
    if (e->kind != Symbol::Defined) {
      error("entry point " + ctx.arg.entry + " is not defined");
      return;
    }
    if (e->csect->smc != XCOFF::XMC_DS)
      warn("entry point " + ctx.arg.entry +
           " is not a function descriptor; AIX starts programs through one");
  }

  writeResult();
}
} // namespace

bool link(ArrayRef<const char *> args, raw_ostream &stdoutOS,
          raw_ostream &stderrOS, bool exitEarly, bool disableOutput) {
  auto *context = new CommonLinkerContext;
  context->e.initialize(stdoutOS, stderrOS, exitEarly, disableOutput);
  context->e.cleanupCallback = []() { ctx.reset(); };
  context->e.logName = args::getFilenameWithoutExe(args[0]);
  context->e.errorLimitExceededMsg = "too many errors emitted, stopping now";

  ctx.reset();
  LinkerDriver().linkerMain(args);
  return errorCount() == 0;
}
} // namespace lld::xcoff
