//===--- IRIX.cpp - SGI IRIX ToolChain --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "IRIX.h"
#include "Arch/Mips.h"
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

static StringRef getIRIXABI(const ArgList &Args, const llvm::Triple &Triple) {
  StringRef CPUName, ABIName;
  mips::getMipsCPUAndABI(Args, Triple, CPUName, ABIName);
  return ABIName;
}

StringRef IRIX::getLibDirName(const ArgList &Args,
                              const llvm::Triple &Triple) {
  StringRef ABI = getIRIXABI(Args, Triple);
  if (ABI == "n32")
    return "lib32";
  if (ABI == "n64")
    return "lib64";
  return "lib";
}

IRIX::IRIX(const Driver &D, const llvm::Triple &Triple, const ArgList &Args)
    : ToolChain(D, Triple, Args) {
  getProgramPaths().push_back(getDriver().Dir);
  StringRef LibDir = getLibDirName(Args, Triple);
  std::string SysRoot = computeSysRoot();
  for (const char *Prefix : {"usr", ""}) {
    SmallString<128> Dir(SysRoot);
    llvm::sys::path::append(Dir, Prefix, LibDir);
    getFilePaths().push_back(std::string(Dir));
  }
}

std::string IRIX::computeSysRoot() const {
  if (!getDriver().SysRoot.empty())
    return getDriver().SysRoot;
  SmallString<128> Dir(getDriver().Dir);
  llvm::sys::path::append(Dir, "..", "mips-sgi-irix");
  return std::string(Dir);
}

void IRIX::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
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
  llvm::sys::path::append(Dir, "usr", "include");
  addSystemInclude(DriverArgs, CC1Args, Dir);
}

void IRIX::AddClangCXXStdlibIncludeArgs(const ArgList &DriverArgs,
                                        ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc, options::OPT_nostdlibinc,
                        options::OPT_nostdincxx))
    return;
  SmallString<128> Dir(computeSysRoot());
  llvm::sys::path::append(Dir, "usr", "include", "c++", "v1");
  addSystemInclude(DriverArgs, CC1Args, Dir);
}

Tool *IRIX::buildLinker() const { return new tools::irix::Linker(*this); }

void irix::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                const InputInfo &Output,
                                const InputInfoList &Inputs,
                                const ArgList &Args,
                                const char *LinkingOutput) const {
  const ToolChain &TC = getToolChain();
  const Driver &D = TC.getDriver();
  const llvm::Triple &Triple = TC.getTriple();
  ArgStringList CmdArgs;

  StringRef ABI = getIRIXABI(Args, Triple);
  bool Relocatable = Args.hasArg(options::OPT_r);
  bool Static = Args.hasArg(options::OPT_static);
  bool Shared = Args.hasArg(options::OPT_shared);

  CmdArgs.push_back("-m");
  if (ABI == "n32")
    CmdArgs.push_back("elf32btsmipn32");
  else if (ABI == "n64")
    CmdArgs.push_back("elf64btsmip");
  else
    CmdArgs.push_back("elf32btsmip");

  if (Relocatable) {
    CmdArgs.push_back("-r");
  } else {
    // IRIX's kernel and rld predate the GNU extensions: a SysV hash table
    // only, and no build ID, PT_GNU_RELRO or PT_GNU_STACK.
    CmdArgs.push_back("--hash-style=sysv");
    CmdArgs.push_back("--build-id=none");
    CmdArgs.push_back("-z");
    CmdArgs.push_back("norelro");
    CmdArgs.push_back("-z");
    CmdArgs.push_back("nognustack");
    if (Static) {
      CmdArgs.push_back("-static");
    } else if (Shared) {
      CmdArgs.push_back("-shared");
    } else {
      CmdArgs.push_back("-dynamic-linker");
      CmdArgs.push_back(ABI == "n32"   ? "/usr/lib32/rld"
                        : ABI == "n64" ? "/usr/lib64/rld"
                                       : "/usr/lib/rld");
    }
    // Where SGI's linker put programs: 0x400000 for o32, 0x10000000 for n32
    // and 64.
    if (!Shared) {
      CmdArgs.push_back("--image-base");
      CmdArgs.push_back(ABI == "o32" ? "0x400000" : "0x10000000");
    }
    CmdArgs.push_back("-e");
    CmdArgs.push_back("__start");
  }
  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  bool StartFiles = !Relocatable && !Shared &&
                    !Args.hasArg(options::OPT_nostdlib,
                                 options::OPT_nostartfiles);
  if (StartFiles)
    CmdArgs.push_back(Args.MakeArgString(TC.GetFilePath("crt1.o")));

  Args.addAllArgs(CmdArgs,
                  {options::OPT_L, options::OPT_u, options::OPT_T_Group,
                   options::OPT_s, options::OPT_t, options::OPT_rpath});
  TC.AddFilePathLibArgs(Args, CmdArgs);
  AddLinkerInputs(TC, Inputs, Args, CmdArgs, JA);

  if (!Relocatable &&
      !Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
    if (D.CCCIsCXX() && TC.ShouldLinkCXXStdlib(Args))
      TC.AddCXXStdlibLibArgs(Args, CmdArgs);
    if (Static)
      CmdArgs.push_back("--start-group");
    CmdArgs.push_back("-lc");
    // The builtins from the resource directory, or else from the sysroot,
    // where the clang-retro package puts them.
    std::string RT = TC.getCompilerRT(Args, "builtins");
    if (!TC.getVFS().exists(RT))
      RT = TC.GetFilePath("libclang_rt.builtins.a");
    if (TC.getVFS().exists(RT))
      CmdArgs.push_back(Args.MakeArgString(RT));
    if (Static)
      CmdArgs.push_back("--end-group");
  }

  if (StartFiles) {
    std::string Crtn = TC.GetFilePath("crtn.o");
    if (Crtn != "crtn.o")
      CmdArgs.push_back(Args.MakeArgString(Crtn));
  }

  C.addCommand(std::make_unique<Command>(
      JA, *this, ResponseFileSupport::AtFileCurCP(),
      Args.MakeArgString(TC.GetLinkerPath()), CmdArgs, Inputs, Output));
}
