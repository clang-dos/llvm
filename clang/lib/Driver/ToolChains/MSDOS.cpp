//===--- MSDOS.cpp - 16-bit DOS ToolChain Implementations -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MSDOS.h"
#include "Arch/X86.h"
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
using namespace clang;
using namespace llvm::opt;

MSDOS::MSDOS(const Driver &D, const llvm::Triple &Triple, const ArgList &Args)
    : ToolChain(D, Triple, Args),
      MemoryModel(x86::getI8086MemoryModel(D, Args).str()) {
  getProgramPaths().push_back(getDriver().Dir);

  // Libraries for the memory model first, then shared ones.
  SmallString<128> LibDir(computeSysRoot());
  llvm::sys::path::append(LibDir, "lib");
  SmallString<128> ModelDir(LibDir);
  llvm::sys::path::append(ModelDir, MemoryModel);
  getFilePaths().push_back(std::string(ModelDir));
  getFilePaths().push_back(std::string(LibDir));
}

std::string MSDOS::computeSysRoot() const {
  if (!getDriver().SysRoot.empty())
    return getDriver().SysRoot;
  SmallString<128> Dir(getDriver().Dir);
  llvm::sys::path::append(Dir, "..", "i8086-msdos");
  return std::string(Dir);
}

void MSDOS::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
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

Tool *MSDOS::buildLinker() const { return new tools::msdos::Linker(*this); }

/// Watcom linkers join their arguments into one directive string, so a file
/// name with spaces or commas must be quoted.
static const char *quoteName(const ArgList &Args, StringRef Name) {
  if (Name.find_first_of(" ,=") == StringRef::npos)
    return Args.MakeArgString(Name);
  return Args.MakeArgString("'" + Name + "'");
}

/// Find the linker: --ld-path=, then -fuse-ld=<name> (wlink, jwlink or a
/// path), then wlink or jwlink on the program path.
static std::string getWatcomLinkerPath(const ToolChain &TC,
                                       const ArgList &Args) {
  if (Args.hasArg(options::OPT_ld_path_EQ))
    return TC.GetLinkerPath();
  if (const Arg *A = Args.getLastArg(options::OPT_fuse_ld_EQ)) {
    StringRef Name = A->getValue();
    if (llvm::sys::path::has_parent_path(Name))
      return std::string(Name);
    return TC.GetProgramPath(Args.MakeArgString(Name));
  }
  for (const char *Name : {"wlink", "jwlink"}) {
    std::string Path = TC.GetProgramPath(Name);
    if (llvm::sys::fs::can_execute(Path))
      return Path;
  }
  return TC.GetProgramPath("wlink");
}

/// Find a library given with -l<name> or used by default: <name>.lib,
/// lib<name>.a or lib<name>.lib in the library paths.
static std::optional<std::string>
findLibrary(const ToolChain &TC, const ArgList &Args, StringRef Name) {
  SmallVector<std::string, 8> Dirs;
  for (const Arg *A : Args.filtered(options::OPT_L))
    Dirs.push_back(A->getValue());
  for (const std::string &Dir : TC.getFilePaths())
    Dirs.push_back(Dir);
  for (const std::string &Dir : Dirs)
    for (const std::string &File :
         {(Name + ".lib").str(), ("lib" + Name + ".a").str(),
          ("lib" + Name + ".lib").str()}) {
      SmallString<128> Path(Dir);
      llvm::sys::path::append(Path, File);
      if (llvm::sys::fs::exists(Path))
        return std::string(Path);
    }
  return std::nullopt;
}

void msdos::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                 const InputInfo &Output,
                                 const InputInfoList &Inputs,
                                 const ArgList &Args,
                                 const char *LinkingOutput) const {
  const auto &TC = static_cast<const toolchains::MSDOS &>(getToolChain());
  ArgStringList CmdArgs;

  // The tiny model gives a .COM program, the others an MZ executable.
  CmdArgs.push_back("format");
  CmdArgs.push_back("dos");
  bool IsCOM = TC.getMemoryModel() == "tiny";
  if (IsCOM)
    CmdArgs.push_back("com");
  CmdArgs.push_back("option");
  CmdArgs.push_back("quiet");
  // DOS ignores a stack segment in a .COM program; the startup code may still
  // define an empty one to mark the end of BSS.
  if (IsCOM) {
    CmdArgs.push_back("disable");
    CmdArgs.push_back("1087");
  }

  assert(Output.isFilename() && "unexpected linker output");
  CmdArgs.push_back("name");
  CmdArgs.push_back(quoteName(Args, Output.getFilename()));

  for (const Arg *A : Args.filtered(options::OPT_L)) {
    CmdArgs.push_back("libpath");
    CmdArgs.push_back(quoteName(Args, A->getValue()));
  }

  // The startup code goes first, so that its segments set the order.
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles)) {
    CmdArgs.push_back("file");
    CmdArgs.push_back(quoteName(Args, TC.GetFilePath("crt0.obj")));
  }

  for (const InputInfo &II : Inputs) {
    if (II.isFilename()) {
      CmdArgs.push_back("file");
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

  // Default libraries are linked when the sysroot has them.
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
    for (StringRef Name : {"c", "clang_rt.builtins"})
      if (std::optional<std::string> Path = findLibrary(TC, Args, Name)) {
        CmdArgs.push_back("library");
        CmdArgs.push_back(quoteName(Args, *Path));
      }
  }

  std::string Linker = getWatcomLinkerPath(TC, Args);
  C.addCommand(std::make_unique<Command>(JA, *this, ResponseFileSupport::None(),
                                         Args.MakeArgString(Linker), CmdArgs,
                                         Inputs, Output));
}
