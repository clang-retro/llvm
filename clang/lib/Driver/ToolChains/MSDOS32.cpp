//===--- MSDOS32.cpp - 32-bit DOS ToolChain Implementations -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MSDOS32.h"
#include "MSDOS.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Options/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang::driver::tools::watcom;
using namespace clang;
using namespace llvm::opt;

/// Makes the toolchain: the programs next to clang, and the libraries in
/// <sysroot>/lib.
MSDOS32::MSDOS32(const Driver &D, const llvm::Triple &Triple,
                 const ArgList &Args)
    : ToolChain(D, Triple, Args) {
  getProgramPaths().push_back(getDriver().Dir);
  SmallString<128> LibDir(computeSysRoot());
  llvm::sys::path::append(LibDir, "lib");
  getFilePaths().push_back(std::string(LibDir));
}

/// Gives the sysroot: --sysroot=, or i386-msdos next to bin.
std::string MSDOS32::computeSysRoot() const {
  if (!getDriver().SysRoot.empty())
    return getDriver().SysRoot;
  SmallString<128> Dir(getDriver().Dir);
  llvm::sys::path::append(Dir, "..", "i386-msdos");
  return std::string(Dir);
}

/// Adds the include directories: clang's own headers, then
/// <sysroot>/include.
void MSDOS32::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
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

/// Adds the libc++ headers, in <sysroot>/include/c++/v1.
void MSDOS32::AddClangCXXStdlibIncludeArgs(const ArgList &DriverArgs,
                                           ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc, options::OPT_nostdlibinc,
                        options::OPT_nostdincxx))
    return;
  SmallString<128> Dir(computeSysRoot());
  llvm::sys::path::append(Dir, "include", "c++", "v1");
  addSystemInclude(DriverArgs, CC1Args, Dir);
}

Tool *MSDOS32::buildLinker() const { return new tools::msdos32::Linker(*this); }

/// Makes the link command: an LE executable with the DOS/32A extender
/// (<sysroot>/lib/dos32a.exe) as its stub, the startup code, the inputs and
/// the default libraries.
void msdos32::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                   const InputInfo &Output,
                                   const InputInfoList &Inputs,
                                   const ArgList &Args,
                                   const char *LinkingOutput) const {
  const ToolChain &TC = getToolChain();
  ArgStringList CmdArgs;

  CmdArgs.append({"format", "os2", "le"});
  CmdArgs.append({"option", "quiet", "option", "eliminate"});

  assert(Output.isFilename() && "unexpected linker output");
  CmdArgs.push_back("name");
  CmdArgs.push_back(quoteName(Args, Output.getFilename()));

  for (const Arg *A : Args.filtered(options::OPT_L)) {
    CmdArgs.push_back("libpath");
    CmdArgs.push_back(quoteName(Args, A->getValue()));
  }

  // The extender is the stub: DOS starts it, and it loads the LE program
  // that follows it in the file. It has its own DPMI host for plain DOS
  // (raw, XMS or VCPI) and uses the DPMI host that is already there, as in
  // a Windows DOS box. Without dos32a.exe in the sysroot, the linker's
  // default stub only prints that the program needs an extender.
  std::string Stub = TC.GetFilePath("dos32a.exe");
  if (llvm::sys::fs::exists(Stub)) {
    CmdArgs.push_back("option");
    CmdArgs.push_back(Args.MakeArgString("stub=" + Stub));
  }

  // The stack (the .stack section of crt0.o).
  CmdArgs.append({"option", "stack=65536"});

  // The startup code goes first, and is the entry point.
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles)) {
    CmdArgs.push_back("file");
    CmdArgs.push_back(quoteName(Args, TC.GetFilePath("crt0.o")));
    CmdArgs.append({"option", "start=_start"});
  }

  for (const InputInfo &II : Inputs) {
    if (II.isFilename()) {
      StringRef Ext = llvm::sys::path::extension(II.getFilename());
      bool IsLib =
          Ext.equals_insensitive(".a") || Ext.equals_insensitive(".lib");
      CmdArgs.push_back(IsLib ? "library" : "file");
      CmdArgs.push_back(quoteName(Args, II.getFilename()));
      continue;
    }
    if (!II.isInputArg())
      continue;
    const Arg &A = II.getInputArg();
    if (A.getOption().matches(options::OPT_l)) {
      StringRef Name = A.getValue();
      CmdArgs.push_back("library");
      if (std::optional<std::string> Path = findLibrary(TC, Args, Name))
        CmdArgs.push_back(quoteName(Args, *Path));
      else
        CmdArgs.push_back(quoteName(Args, Name));
      continue;
    }
    // -Wl, and -Xlinker values are linker directives.
    A.renderAsInput(Args, CmdArgs);
  }

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
    SmallVector<StringRef, 5> Names;
    // C++ programs also get the C++ library and the SjLj unwinder.
    if (TC.ShouldLinkCXXStdlib(Args))
      Names.append({"c++", "c++abi", "unwind"});
    Names.append({"c", "clang_rt.builtins"});
    for (StringRef Name : Names)
      if (std::optional<std::string> Path = findLibrary(TC, Args, Name)) {
        CmdArgs.push_back("library");
        CmdArgs.push_back(quoteName(Args, *Path));
      }
  }

  std::string Linker = getLinkerPath(TC, Args);
  C.addCommand(std::make_unique<Command>(JA, *this, ResponseFileSupport::None(),
                                         Args.MakeArgString(Linker), CmdArgs,
                                         Inputs, Output));
}
