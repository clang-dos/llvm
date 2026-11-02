//===--- AmigaOS.cpp - AmigaOS ToolChain ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AmigaOS.h"
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

AmigaOS::AmigaOS(const Driver &D, const llvm::Triple &Triple,
                 const ArgList &Args)
    : ToolChain(D, Triple, Args) {
  getProgramPaths().push_back(getDriver().Dir);
  SmallString<128> LibDir(computeSysRoot());
  llvm::sys::path::append(LibDir, "lib");
  getFilePaths().push_back(std::string(LibDir));
}

std::string AmigaOS::computeSysRoot() const {
  if (!getDriver().SysRoot.empty())
    return getDriver().SysRoot;
  SmallString<128> Dir(getDriver().Dir);
  llvm::sys::path::append(Dir, "..", "m68k-amigaos");
  return std::string(Dir);
}

void AmigaOS::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
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

void AmigaOS::AddClangCXXStdlibIncludeArgs(const ArgList &DriverArgs,
                                           ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc, options::OPT_nostdlibinc,
                        options::OPT_nostdincxx))
    return;
  SmallString<128> Dir(computeSysRoot());
  llvm::sys::path::append(Dir, "include", "c++", "v1");
  addSystemInclude(DriverArgs, CC1Args, Dir);
}

Tool *AmigaOS::buildLinker() const { return new tools::amigaos::Linker(*this); }

void amigaos::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                   const InputInfo &Output,
                                   const InputInfoList &Inputs,
                                   const ArgList &Args,
                                   const char *LinkingOutput) const {
  const ToolChain &TC = getToolChain();
  const Driver &D = TC.getDriver();
  ArgStringList CmdArgs;

  // Link to an m68k ELF program, keeping its relocations (--emit-relocs) so
  // llvm-elf2hunk can turn them into HUNK_RELOC32 tables. -r and -c stop at
  // the ELF file.
  bool Relocatable = Args.hasArg(options::OPT_r);
  const char *ElfOut = Output.getFilename();
  if (!Relocatable) {
    std::string Tmp =
        D.GetTemporaryPath(llvm::sys::path::stem(Output.getFilename()), "elf");
    ElfOut = C.addTempFile(Args.MakeArgString(Tmp));
  }

  CmdArgs.push_back("-m");
  CmdArgs.push_back("m68kelf");
  if (Relocatable) {
    CmdArgs.push_back("-r");
  } else {
    CmdArgs.push_back("-static");
    CmdArgs.push_back("--emit-relocs");
    CmdArgs.push_back("--no-rosegment");
    CmdArgs.push_back("-z");
    CmdArgs.push_back("norelro");
    CmdArgs.push_back("--build-id=none");
    CmdArgs.push_back("-e");
    CmdArgs.push_back("_start");
  }
  CmdArgs.push_back("-o");
  CmdArgs.push_back(ElfOut);

  // The startup code must come first: LoadSeg() starts the program at the
  // first byte of its first hunk.
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
    CmdArgs.push_back("-lamiga");
    std::string RT = TC.getCompilerRT(Args, "builtins");
    if (TC.getVFS().exists(RT))
      CmdArgs.push_back(Args.MakeArgString(RT));
    CmdArgs.push_back("--end-group");
  }

  C.addCommand(std::make_unique<Command>(
      JA, *this, ResponseFileSupport::AtFileCurCP(),
      Args.MakeArgString(TC.GetLinkerPath()), CmdArgs, Inputs, Output));

  if (Relocatable)
    return;

  ArgStringList HunkArgs;
  HunkArgs.push_back(ElfOut);
  HunkArgs.push_back("-o");
  HunkArgs.push_back(Output.getFilename());
  if (!Args.hasArg(options::OPT_s))
    HunkArgs.push_back("--symbols");
  C.addCommand(std::make_unique<Command>(
      JA, *this, ResponseFileSupport::None(),
      Args.MakeArgString(TC.GetProgramPath("llvm-elf2hunk")), HunkArgs, Inputs,
      Output));
}
