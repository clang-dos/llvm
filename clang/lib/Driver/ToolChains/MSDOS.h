//===--- MSDOS.h - 16-bit DOS ToolChain Implementations ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_MSDOS_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_MSDOS_H

#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace toolchains {

/// Toolchain for 16-bit real-mode DOS (i8086-*-msdos). Objects are OMF, and
/// programs are linked with a Watcom-style linker (wlink or JWlink) into an
/// MZ .EXE, or a .COM file for the tiny memory model.
///
/// The sysroot (--sysroot, or <install>/i8086-msdos by default) holds
/// include/ and one library directory per memory model, lib/<model>/, with
/// crt0.obj and the default libraries.
class LLVM_LIBRARY_VISIBILITY MSDOS : public ToolChain {
public:
  MSDOS(const Driver &D, const llvm::Triple &Triple,
        const llvm::opt::ArgList &Args);

  bool isPICDefault() const override { return false; }
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
  const char *getDefaultLinker() const override { return "wlink"; }

  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;

  /// The memory model chosen with -mcmodel= (small by default).
  StringRef getMemoryModel() const { return MemoryModel; }

protected:
  Tool *buildLinker() const override;
  std::string computeSysRoot() const override;

private:
  std::string MemoryModel;
};

} // end namespace toolchains

namespace tools {
namespace msdos {

class LLVM_LIBRARY_VISIBILITY Linker final : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("msdos::Linker", "wlink", TC) {}
  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

} // end namespace msdos
} // end namespace tools
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_MSDOS_H
