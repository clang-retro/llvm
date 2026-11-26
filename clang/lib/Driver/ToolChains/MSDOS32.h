//===--- MSDOS32.h - 32-bit DOS ToolChain Implementations -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_MSDOS32_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_MSDOS32_H

#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace toolchains {

/// 32-bit DOS (i386-pc-msdos): ELF objects, linked into an LE executable by a
/// Watcom linker (wlink or JWlink). The DOS/32A extender is the stub of the
/// executable, so the program runs without a separate extender or DPMI host.
/// 16-bit DOS uses the MSDOS toolchain.
class LLVM_LIBRARY_VISIBILITY MSDOS32 : public ToolChain {
public:
  MSDOS32(const Driver &D, const llvm::Triple &Triple,
          const llvm::opt::ArgList &Args);

  bool isPICDefault() const override { return false; }
  bool isPIEDefault(const llvm::opt::ArgList &Args) const override {
    return false;
  }
  // LE fixups relocate the code: there is no GOT.
  bool isPICDefaultForced() const override { return true; }
  UnwindTableLevel
  getDefaultUnwindTableLevel(const llvm::opt::ArgList &Args) const override {
    return UnwindTableLevel::None;
  }
  RuntimeLibType GetDefaultRuntimeLibType() const override {
    return ToolChain::RLT_CompilerRT;
  }
  const char *getDefaultLinker() const override { return "wlink"; }
  bool IsIntegratedAssemblerDefault() const override { return true; }
  // C++ exceptions unwind through SjLj function contexts, as on 32-bit OS/2:
  // an LE file keeps no .eh_frame for an unwinder.
  llvm::ExceptionHandling
  GetExceptionModel(const llvm::opt::ArgList &Args) const override {
    return llvm::ExceptionHandling::SjLj;
  }

  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;
  CXXStdlibType GetDefaultCXXStdlibType() const override {
    return ToolChain::CST_Libcxx;
  }
  void AddClangCXXStdlibIncludeArgs(
      const llvm::opt::ArgList &DriverArgs,
      llvm::opt::ArgStringList &CC1Args) const override;

protected:
  Tool *buildLinker() const override;
  std::string computeSysRoot() const override;
};

} // end namespace toolchains

namespace tools {
namespace msdos32 {

/// The link job of the MSDOS32 toolchain: a Watcom linker that writes an LE
/// executable with the DOS/32A stub.
class LLVM_LIBRARY_VISIBILITY Linker final : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("msdos32::Linker", "wlink", TC) {}
  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

} // end namespace msdos32
} // end namespace tools

} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_MSDOS32_H
