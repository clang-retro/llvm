//===--- SCO.cpp - SCO UnixWare and OpenServer ToolChain --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SCO.h"
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

SCO::SCO(const Driver &D, const llvm::Triple &Triple, const ArgList &Args)
    : ToolChain(D, Triple, Args) {
  getProgramPaths().push_back(getDriver().Dir);
  std::string SysRoot = computeSysRoot();
  for (const char *Prefix : {"usr", ""}) {
    SmallString<128> Dir(SysRoot);
    llvm::sys::path::append(Dir, Prefix, "lib");
    getFilePaths().push_back(std::string(Dir));
  }
}

std::string SCO::computeSysRoot() const {
  if (!getDriver().SysRoot.empty())
    return getDriver().SysRoot;
  SmallString<128> Dir(getDriver().Dir);
  llvm::sys::path::append(Dir, "..", "i386-pc-sco");
  return std::string(Dir);
}

void SCO::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
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

void SCO::AddClangCXXStdlibIncludeArgs(const ArgList &DriverArgs,
                                       ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc, options::OPT_nostdlibinc,
                        options::OPT_nostdincxx))
    return;
  SmallString<128> Dir(computeSysRoot());
  llvm::sys::path::append(Dir, "usr", "include", "c++", "v1");
  addSystemInclude(DriverArgs, CC1Args, Dir);
}

Tool *SCO::buildLinker() const { return new tools::sco::Linker(*this); }

void sco::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                               const InputInfo &Output,
                               const InputInfoList &Inputs, const ArgList &Args,
                               const char *LinkingOutput) const {
  const ToolChain &TC = getToolChain();
  const Driver &D = TC.getDriver();
  ArgStringList CmdArgs;

  bool Relocatable = Args.hasArg(options::OPT_r);

  CmdArgs.push_back("-m");
  CmdArgs.push_back("elf_i386");

  if (Relocatable) {
    CmdArgs.push_back("-r");
  } else {
    // Static programs only: SCO's kernels and linkers predate the GNU
    // extensions, so no build ID, PT_GNU_RELRO or PT_GNU_STACK, and the
    // program goes where SVR4 linkers put it.
    CmdArgs.push_back("-static");
    CmdArgs.push_back("--build-id=none");
    CmdArgs.push_back("-z");
    CmdArgs.push_back("norelro");
    CmdArgs.push_back("-z");
    CmdArgs.push_back("nognustack");
    CmdArgs.push_back("--image-base");
    CmdArgs.push_back("0x8048000");
  }
  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  bool StartFiles = !Relocatable && !Args.hasArg(options::OPT_nostdlib,
                                                 options::OPT_nostartfiles);
  if (StartFiles)
    CmdArgs.push_back(Args.MakeArgString(TC.GetFilePath("crt1.o")));

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
    // The builtins from the resource directory, or else from the sysroot,
    // where the clang-retro package puts them.
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
}
