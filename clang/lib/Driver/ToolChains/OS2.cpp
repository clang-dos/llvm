//===--- OS2.cpp - 32-bit OS/2 ToolChain Implementations --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "OS2.h"
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

OS2::OS2(const Driver &D, const llvm::Triple &Triple, const ArgList &Args)
    : ToolChain(D, Triple, Args) {
  getProgramPaths().push_back(getDriver().Dir);
  SmallString<128> LibDir(computeSysRoot());
  llvm::sys::path::append(LibDir, "lib");
  getFilePaths().push_back(std::string(LibDir));
}

/// The sysroot: --sysroot=, or i386-os2 next to bin.
std::string OS2::computeSysRoot() const {
  if (!getDriver().SysRoot.empty())
    return getDriver().SysRoot;
  SmallString<128> Dir(getDriver().Dir);
  llvm::sys::path::append(Dir, "..", "i386-os2");
  return std::string(Dir);
}

void OS2::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
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

void OS2::AddClangCXXStdlibIncludeArgs(const ArgList &DriverArgs,
                                       ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc, options::OPT_nostdlibinc,
                        options::OPT_nostdincxx))
    return;
  SmallString<128> Dir(computeSysRoot());
  llvm::sys::path::append(Dir, "include", "c++", "v1");
  addSystemInclude(DriverArgs, CC1Args, Dir);
}

Tool *OS2::buildLinker() const { return new tools::os2::Linker(*this); }

void os2::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                               const InputInfo &Output,
                               const InputInfoList &Inputs, const ArgList &Args,
                               const char *LinkingOutput) const {
  const ToolChain &TC = getToolChain();
  ArgStringList CmdArgs;

  // An LX program, which runs in a window of the Presentation Manager as
  // text-mode programs do (-mwindows for a PM program), or a DLL with
  // -shared, initialized and terminated for each process that uses it.
  bool IsDLL = Args.hasArg(options::OPT_shared);
  CmdArgs.append({"format", "os2", "lx"});
  if (IsDLL)
    CmdArgs.append({"dll", "initinstance", "terminstance"});
  else
    CmdArgs.push_back(Args.hasArg(options::OPT_mwindows) ? "pm"
                                                         : "pmcompatible");
  CmdArgs.append({"option", "quiet", "option", "eliminate"});

  assert(Output.isFilename() && "unexpected linker output");
  CmdArgs.push_back("name");
  CmdArgs.push_back(quoteName(Args, Output.getFilename()));

  for (const Arg *A : Args.filtered(options::OPT_L)) {
    CmdArgs.push_back("libpath");
    CmdArgs.push_back(quoteName(Args, A->getValue()));
  }

  // The stack of the first thread (the .stack section of crt0.o). A DLL
  // runs on its caller's, and has data of its own in each process that
  // uses it, as the C library's state must be.
  if (!IsDLL)
    CmdArgs.append({"option", "stack=65536"});
  else
    CmdArgs.append({"segment", "type", "data", "nonshared"});

  // The startup code goes first, and is the entry point.
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles)) {
    CmdArgs.push_back("file");
    CmdArgs.push_back(
        quoteName(Args, TC.GetFilePath(IsDLL ? "dllcrt0.o" : "crt0.o")));
    CmdArgs.push_back("option");
    CmdArgs.push_back(IsDLL ? "start=_dll_start" : "start=_start");
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
    if (TC.ShouldLinkCXXStdlib(Args))
      Names.append({"c++", "c++abi", "unwind"});
    Names.append({"c", "clang_rt.builtins"});
    for (StringRef Name : Names)
      if (std::optional<std::string> Path = findLibrary(TC, Args, Name)) {
        CmdArgs.push_back("library");
        CmdArgs.push_back(quoteName(Args, *Path));
      }
    // The OS/2 API: import directives for DOSCALLS, the Presentation Manager
    // and the other system DLLs. The linker keeps only the imports that are
    // used.
    std::string Imports = TC.GetFilePath("os2.lnk");
    if (llvm::sys::fs::exists(Imports))
      CmdArgs.push_back(Args.MakeArgString("@" + Imports));
  }

  std::string Linker = getLinkerPath(TC, Args);
  C.addCommand(std::make_unique<Command>(JA, *this, ResponseFileSupport::None(),
                                         Args.MakeArgString(Linker), CmdArgs,
                                         Inputs, Output));
}
