//===--- IRIX.h - SGI IRIX ToolChain ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_IRIX_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_IRIX_H

#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace toolchains {

/// SGI IRIX 5.x and 6.x on MIPS: o32 (mips-sgi-irix), and n32 or 64
/// (mips64-sgi-irix with -mabi=n32 or -mabi=64). Headers and libraries come
/// from a sysroot laid out like an IRIX system, by default
/// <bin>/../mips-sgi-irix: usr/include, and usr/lib, usr/lib32 or usr/lib64
/// for the three ABIs. Programs are linked with ld.lld, dynamically against
/// rld unless -static is given.
class LLVM_LIBRARY_VISIBILITY IRIX : public ToolChain {
public:
  IRIX(const Driver &D, const llvm::Triple &Triple,
       const llvm::opt::ArgList &Args);

  // IRIX compilers make PIC (-KPIC) by default and so do we.
  bool isPICDefault() const override { return true; }
  bool isPIEDefault(const llvm::opt::ArgList &Args) const override {
    return false;
  }
  bool isPICDefaultForced() const override { return false; }
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

  /// The library directory for the ABI: "lib" (o32), "lib32" (n32) or
  /// "lib64" (64).
  static StringRef getLibDirName(const llvm::opt::ArgList &Args,
                                 const llvm::Triple &Triple);

protected:
  Tool *buildLinker() const override;
  std::string computeSysRoot() const override;
};

} // end namespace toolchains

namespace tools {
namespace irix {

class LLVM_LIBRARY_VISIBILITY Linker final : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("irix::Linker", "ld.lld", TC) {}
  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

} // end namespace irix
} // end namespace tools
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_IRIX_H
