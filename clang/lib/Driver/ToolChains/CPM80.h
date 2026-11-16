//===--- CPM80.h - CP/M-80 ToolChain ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_CPM80_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_CPM80_H

#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace toolchains {

/// CP/M-80 (TOS 1.x to 4.x and EmuTOS) on 68k. Headers and libraries come
/// from a sysroot, by default <bin>/../m68k-atari-tos. Programs are linked as
/// m68k ELF with ld.lld and the sysroot's tos.ld script, keeping relocations,
/// then turned into a PRG executable by llvm-elf2tos.
class LLVM_LIBRARY_VISIBILITY CPM80 : public ToolChain {
public:
  CPM80(const Driver &D, const llvm::Triple &Triple,
           const llvm::opt::ArgList &Args);

  // A .COM program runs at 0x100 and is not relocated.
  bool isPICDefault() const override { return false; }
  bool isPIEDefault(const llvm::opt::ArgList &Args) const override {
    return false;
  }
  bool isPICDefaultForced() const override { return true; }
  bool IsIntegratedAssemblerDefault() const override { return true; }
  UnwindTableLevel
  getDefaultUnwindTableLevel(const llvm::opt::ArgList &Args) const override {
    return UnwindTableLevel::None;
  }
  RuntimeLibType GetDefaultRuntimeLibType() const override {
    return ToolChain::RLT_CompilerRT;
  }
  CXXStdlibType GetDefaultCXXStdlibType() const override {
    return ToolChain::CST_Libcxx;
  }
  const char *getDefaultLinker() const override { return "ld.lld"; }

  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;
  void AddClangCXXStdlibIncludeArgs(
      const llvm::opt::ArgList &DriverArgs,
      llvm::opt::ArgStringList &CC1Args) const override;

protected:
  Tool *buildLinker() const override;
  std::string computeSysRoot() const override;
};

} // end namespace toolchains

namespace tools {
namespace cpm80 {

class LLVM_LIBRARY_VISIBILITY Linker final : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("cpm80::Linker", "ld.lld", TC) {}
  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

} // end namespace cpm80
} // end namespace tools
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_CPM80_H
