//===--- CPM80.cpp - CP/M-80 ToolChain --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CPM80.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Options/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

CPM80::CPM80(const Driver &D, const llvm::Triple &Triple, const ArgList &Args)
    : ToolChain(D, Triple, Args) {
  getProgramPaths().push_back(getDriver().Dir);
  SmallString<128> LibDir(computeSysRoot());
  llvm::sys::path::append(LibDir, "lib");
  getFilePaths().push_back(std::string(LibDir));
}

// The Z80 and the 8080 have their own sysroots: z80-cpm and i8080-cpm. The
// 8080 libraries run on a Z80 too.
std::string CPM80::computeSysRoot() const {
  if (!getDriver().SysRoot.empty())
    return getDriver().SysRoot;
  SmallString<128> Dir(getDriver().Dir);
  llvm::sys::path::append(Dir, "..",
                          getTriple().getArch() == llvm::Triple::z80
                              ? "z80-cpm"
                              : "i8080-cpm");
  return std::string(Dir);
}

void CPM80::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                      ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc))
    return;
  if (!DriverArgs.hasArg(options::OPT_nobuiltininc)) {
    SmallString<128> Dir(getDriver().ResourceDir);
    llvm::sys::path::append(Dir, "include");
    addSystemInclude(DriverArgs, CC1Args, Dir);
  }
  if (DriverArgs.hasArg(options::OPT_nostdlibinc))
    return;
  SmallString<128> Dir(computeSysRoot());
  llvm::sys::path::append(Dir, "include");
  addSystemInclude(DriverArgs, CC1Args, Dir);
}

void CPM80::AddClangCXXStdlibIncludeArgs(const ArgList &DriverArgs,
                                         ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc, options::OPT_nostdlibinc,
                        options::OPT_nostdincxx))
    return;
  SmallString<128> Dir(computeSysRoot());
  llvm::sys::path::append(Dir, "include", "c++", "v1");
  addSystemInclude(DriverArgs, CC1Args, Dir);
}

Tool *CPM80::buildLinker() const { return new tools::cpm80::Linker(*this); }

void cpm80::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                 const InputInfo &Output,
                                 const InputInfoList &Inputs,
                                 const ArgList &Args,
                                 const char *LinkingOutput) const {
  const ToolChain &TC = getToolChain();
  const Driver &D = TC.getDriver();
  ArgStringList CmdArgs;

  // Link to an ELF program at 0x100, then take its image as the .COM file.
  // -r stops at the ELF file.
  bool Relocatable = Args.hasArg(options::OPT_r);
  const char *ElfOut = Output.getFilename();
  if (!Relocatable) {
    std::string Tmp =
        D.GetTemporaryPath(llvm::sys::path::stem(Output.getFilename()), "elf");
    ElfOut = C.addTempFile(Args.MakeArgString(Tmp));
  }

  CmdArgs.push_back("-m");
  CmdArgs.push_back("elf32z80");
  if (Relocatable) {
    CmdArgs.push_back("-r");
  } else {
    CmdArgs.push_back("-static");
    CmdArgs.push_back("-z");
    CmdArgs.push_back("norelro");
    CmdArgs.push_back("--build-id=none");
    // A TPA is at most about 60 KiB, often much less.
    CmdArgs.push_back("--gc-sections");
    CmdArgs.push_back("-e");
    CmdArgs.push_back("_start");
    // CP/M loads the file at 0x100 and jumps to its first byte: the startup
    // code, then the rest of the code, the read-only data and the data; the
    // bss follows the file. The layout comes from a linker script.
    if (!Args.hasArg(options::OPT_T)) {
      std::string Script = TC.GetFilePath("cpm80.ld");
      if (Script != "cpm80.ld") {
        CmdArgs.push_back("-T");
        CmdArgs.push_back(Args.MakeArgString(Script));
      } else {
        CmdArgs.push_back("--image-base=0x100");
      }
    }
  }
  CmdArgs.push_back("-o");
  CmdArgs.push_back(ElfOut);

  // The startup code must come first.
  if (!Relocatable &&
      !Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles))
    CmdArgs.push_back(Args.MakeArgString(TC.GetFilePath("crt0.o")));

  Args.addAllArgs(CmdArgs,
                  {options::OPT_L, options::OPT_u, options::OPT_T_Group,
                   options::OPT_s, options::OPT_t});
  TC.AddFilePathLibArgs(Args, CmdArgs);
  AddLinkerInputs(TC, Inputs, Args, CmdArgs, JA);

  if (!Relocatable &&
      !Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
    if (D.CCCIsCXX() && TC.ShouldLinkCXXStdlib(Args))
      TC.AddCXXStdlibLibArgs(Args, CmdArgs);
    CmdArgs.push_back("--start-group");
    CmdArgs.push_back("-lc");
    // The builtins from the resource directory, or else from the sysroot.
    std::string RT = TC.getCompilerRT(Args, "builtins");
    if (!TC.getVFS().exists(RT))
      RT = TC.GetFilePath("libclang_rt.builtins.a");
    if (TC.getVFS().exists(RT))
      CmdArgs.push_back(Args.MakeArgString(RT));
    CmdArgs.push_back("--end-group");
  }

  C.addCommand(std::make_unique<Command>(
      JA, *this, ResponseFileSupport::AtFileCurCP(),
      Args.MakeArgString(TC.GetLinkerPath()), CmdArgs, Inputs, Output));

  if (Relocatable)
    return;

  ArgStringList ComArgs;
  ComArgs.push_back("-O");
  ComArgs.push_back("binary");
  ComArgs.push_back(ElfOut);
  ComArgs.push_back(Output.getFilename());
  C.addCommand(std::make_unique<Command>(
      JA, *this, ResponseFileSupport::None(),
      Args.MakeArgString(TC.GetProgramPath("llvm-objcopy")), ComArgs, Inputs,
      Output));
}
