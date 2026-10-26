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

  // Windows runs in protected mode, where segment arithmetic does not work:
  // the huge models are out, and a .COM program is a DOS thing.
  if (Triple.isOSWin16() && (MemoryModel == "tiny" ||
                             StringRef(MemoryModel).starts_with("huge")))
    D.Diag(diag::err_drv_unsupported_option_argument_for_target)
        << "-mcmodel=" << MemoryModel << Triple.str();

  // Libraries for the memory model first, then shared ones. Windows
  // libraries are in lib/win16.
  SmallString<128> LibDir(computeSysRoot());
  llvm::sys::path::append(LibDir, "lib");
  if (Triple.isOSWin16())
    llvm::sys::path::append(LibDir, "win16");
  SmallString<128> ModelDir(LibDir);
  llvm::sys::path::append(ModelDir, MemoryModel);
  // With C++ exceptions (off by default), libc++ and libc++abi built with
  // them, and the unwinder, come from <model>/eh.
  const Arg *EH = Args.getLastArg(
      options::OPT_fcxx_exceptions, options::OPT_fno_cxx_exceptions,
      options::OPT_fexceptions, options::OPT_fno_exceptions);
  if (EH && (EH->getOption().matches(options::OPT_fexceptions) ||
             EH->getOption().matches(options::OPT_fcxx_exceptions))) {
    SmallString<128> EHDir(ModelDir);
    llvm::sys::path::append(EHDir, "eh");
    getFilePaths().push_back(std::string(EHDir));
  }
  // A Windows DLL in the small or medium model links the libraries built
  // with -mwindows-dll, from <model>/dll.
  if (Triple.isOSWin16() &&
      (MemoryModel == "small" || MemoryModel == "medium") &&
      Args.hasArg(options::OPT_shared, options::OPT_mwindows_dll)) {
    SmallString<128> DLLDir(ModelDir);
    llvm::sys::path::append(DLLDir, "dll");
    getFilePaths().push_back(std::string(DLLDir));
  }
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

void MSDOS::AddClangCXXStdlibIncludeArgs(const ArgList &DriverArgs,
                                         ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc, options::OPT_nostdlibinc,
                        options::OPT_nostdincxx))
    return;
  // libc++ in <sysroot>/include/c++/v1, as on other bare targets.
  SmallString<128> Dir(computeSysRoot());
  llvm::sys::path::append(Dir, "include", "c++", "v1");
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

  // The tiny model gives a .COM program, the others an MZ executable. For
  // Windows, an NE program, or a DLL with -shared.
  bool Win16 = TC.getTriple().isOSWin16();
  bool IsDLL = Win16 && Args.hasArg(options::OPT_shared);
  CmdArgs.push_back("format");
  CmdArgs.push_back(Win16 ? "windows" : "dos");
  bool IsCOM = !Win16 && TC.getMemoryModel() == "tiny";
  if (IsCOM)
    CmdArgs.push_back("com");
  if (IsDLL)
    CmdArgs.push_back("dll");
  CmdArgs.push_back("option");
  CmdArgs.push_back("quiet");
  // Drop code that nothing references. With COMDATs this removes unused
  // functions, such as most of an explicitly instantiated libc++ template.
  CmdArgs.push_back("option");
  CmdArgs.push_back("eliminate");
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

  if (Win16) {
    // Windows grows the local heap (LocalAlloc, and malloc in the near data
    // models) from heapsize. A DLL runs on its caller's stack.
    CmdArgs.push_back("option");
    CmdArgs.push_back("heapsize=4096");
    if (!IsDLL) {
      CmdArgs.push_back("option");
      CmdArgs.push_back("stack=8192");
    }

    // A Windows version in the triple, as in i8086-pc-win162.03, goes in the
    // header as the expected version; Windows refuses a program that expects
    // a newer one. Windows 1.x, 2.x and 3.0 in real mode move segments
    // without updating the segment addresses in other segments, so for them
    // the code is fixed in memory, and so is the data when far pointers or
    // code can hold its address. Windows 3.1 and later only run in protected
    // mode, which is all the default build supports: the header says so, and
    // Windows 3.0 then refuses to run the program in real mode.
    VersionTuple Version = TC.getTriple().getOSVersion();
    if (!Version.empty()) {
      CmdArgs.push_back("option");
      unsigned Minor = Version.getMinor().value_or(0);
      CmdArgs.push_back(Args.MakeArgString(
          "version=" + Twine(Version.getMajor()) + (Minor < 10 ? ".0" : ".") +
          Twine(Minor)));
    }
    if (!Version.empty() && Version < VersionTuple(3, 10)) {
      CmdArgs.append({"segment", "type", "code", "fixed"});
      // A DLL's code loads its data segment with "mov ax, DGROUP", which
      // Windows cannot update either, as old .DEF files had DATA FIXED.
      StringRef Model = TC.getMemoryModel();
      if (IsDLL || Model == "compact" || Model == "large")
        CmdArgs.append({"segment", "type", "data", "fixed"});
    } else {
      CmdArgs.push_back("option");
      CmdArgs.push_back("protmode");
    }
  }

  // The startup code goes first, so that its segments set the order. A DLL
  // starts with libentry.obj.
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles)) {
    CmdArgs.push_back("file");
    CmdArgs.push_back(
        quoteName(Args, TC.GetFilePath(IsDLL ? "libentry.obj" : "crt0.obj")));
  }

  bool HaveResources = false;
  for (const InputInfo &II : Inputs) {
    if (II.isFilename()) {
      // Watcom linkers take libraries only with the library directive.
      StringRef Ext = llvm::sys::path::extension(II.getFilename());
      // A Windows program or DLL takes its resources from one .res file
      // (llvm-rc -win16).
      if (Win16 && Ext.equals_insensitive(".res")) {
        if (HaveResources)
          TC.getDriver().Diag(diag::err_drv_msdos_one_res_file);
        HaveResources = true;
        CmdArgs.push_back("option");
        CmdArgs.push_back(
            Args.MakeArgString(Twine("resource=") +
                               quoteName(Args, II.getFilename())));
        continue;
      }
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

  // Default libraries are linked when the sysroot has them.
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
    // C++ programs also get libc++, libc++abi and its unwinder.
    SmallVector<StringRef, 5> Names;
    if (TC.ShouldLinkCXXStdlib(Args))
      Names.append({"c++", "c++abi", "unwind"});
    Names.append({"c", "clang_rt.builtins"});
    for (StringRef Name : Names)
      if (std::optional<std::string> Path = findLibrary(TC, Args, Name)) {
        CmdArgs.push_back("library");
        CmdArgs.push_back(quoteName(Args, *Path));
      }
    // The Windows API: import directives for KERNEL, USER and GDI. The
    // linker keeps only the imports that are used. A program for Windows
    // 1.x, 2.x or 3.0 gets the functions that version has, so that it
    // cannot call one that came later (it would then fail to load), and
    // gets them where that version has them.
    if (Win16) {
      VersionTuple Version = TC.getTriple().getOSVersion();
      const char *List = "windows.lnk";
      if (!Version.empty() && Version < VersionTuple(2))
        List = "windows100.lnk";
      else if (!Version.empty() && Version < VersionTuple(3))
        List = "windows200.lnk";
      else if (!Version.empty() && Version < VersionTuple(3, 10))
        List = "windows300.lnk";
      std::string Imports = TC.GetFilePath(List);
      if (!llvm::sys::fs::exists(Imports))
        Imports = TC.GetFilePath("windows.lnk");
      if (llvm::sys::fs::exists(Imports))
        CmdArgs.push_back(Args.MakeArgString("@" + Imports));
    }
  }

  std::string Linker = getWatcomLinkerPath(TC, Args);
  C.addCommand(std::make_unique<Command>(JA, *this, ResponseFileSupport::None(),
                                         Args.MakeArgString(Linker), CmdArgs,
                                         Inputs, Output));
}
