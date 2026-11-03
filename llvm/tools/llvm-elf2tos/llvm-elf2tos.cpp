//===- llvm-elf2tos.cpp - Convert an m68k ELF program to an Atari PRG -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Turns an m68k ELF executable, linked with --emit-relocs, into an Atari
// TOS program (.PRG, .TOS, .TTP or .ACC) that GEMDOS Pexec() can load.
//
// GEMDOS loads the text, data and bss segments one after another, so the
// program keeps the layout it was linked with: everything from the lowest
// allocated address up to the first writable section is text, the rest of
// the initialized sections are data, and the sections without contents at
// the end are bss. Each 32-bit absolute relocation becomes an entry in the
// PRG fixup table, which GEMDOS adds the load address to. Execution starts
// at the first byte of the text, so the entry point has to be there.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>

using namespace llvm;
using namespace llvm::object;

static cl::OptionCategory Cat("llvm-elf2tos options");
static cl::opt<std::string> InputFile(cl::Positional, cl::Required,
                                      cl::desc("<input ELF>"), cl::cat(Cat));
static cl::opt<std::string> OutputFile("o", cl::Required,
                                       cl::desc("Output PRG file"),
                                       cl::value_desc("file"), cl::cat(Cat));
static cl::opt<bool> Symbols("symbols",
                             cl::desc("Write a DRI symbol table for debuggers"),
                             cl::cat(Cat));
static cl::opt<std::string>
    Flags("flags",
          cl::desc("PRG header flags: a number, or a comma-separated list of "
                   "fastload, ttram (load into TT RAM), ttmalloc (Malloc() "
                   "from TT RAM), private, global, super, readable"),
          cl::value_desc("flags"), cl::init("0"), cl::cat(Cat));
static cl::opt<bool> Verbose("v", cl::desc("Print the segment sizes"),
                             cl::cat(Cat));

static StringRef ToolName;

[[noreturn]] static void fail(const Twine &Msg) {
  WithColor::error(errs(), ToolName) << Msg << "\n";
  exit(1);
}

using ELFT = ELF32BE;
using Elf_Shdr = ELFT::Shdr;

static uint32_t parseFlags(StringRef S) {
  uint32_t V;
  if (!S.getAsInteger(0, V))
    return V;
  V = 0;
  SmallVector<StringRef> Parts;
  S.split(Parts, ',', -1, false);
  for (StringRef P : Parts) {
    P = P.trim();
    if (P == "fastload")
      V |= 1; // Only the bss is cleared, not the rest of the TPA.
    else if (P == "ttram")
      V |= 2;
    else if (P == "ttmalloc")
      V |= 4;
    // Memory protection mode (MiNT), bits 4 and 5.
    else if (P == "private")
      V |= 0x00;
    else if (P == "global")
      V |= 0x10;
    else if (P == "super")
      V |= 0x20;
    else if (P == "readable")
      V |= 0x30;
    else
      fail("unknown PRG flag '" + P + "'");
  }
  return V;
}

static void put16(std::string &Out, uint16_t V) {
  char B[2];
  support::endian::write16be(B, V);
  Out.append(B, 2);
}

static void put32(std::string &Out, uint32_t V) {
  char B[4];
  support::endian::write32be(B, V);
  Out.append(B, 4);
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);
  ToolName = argv[0];
  cl::HideUnrelatedOptions(Cat);
  cl::ParseCommandLineOptions(
      argc, argv, "Convert an m68k ELF program to an Atari TOS executable\n");
  uint32_t PrgFlags = parseFlags(Flags);

  auto BufOrErr = MemoryBuffer::getFile(InputFile);
  if (!BufOrErr)
    fail(InputFile + ": " + BufOrErr.getError().message());
  auto ObjOrErr = ELFFile<ELFT>::create((*BufOrErr)->getBuffer());
  if (!ObjOrErr)
    fail(InputFile + ": " + toString(ObjOrErr.takeError()));
  const ELFFile<ELFT> &Obj = *ObjOrErr;
  const auto &Hdr = Obj.getHeader();
  if (Hdr.e_machine != ELF::EM_68K)
    fail(InputFile + ": not an m68k ELF file");
  if (Hdr.e_type != ELF::ET_EXEC)
    fail(InputFile + ": not an executable; link it first (with --emit-relocs)");

  auto SecsOrErr = Obj.sections();
  if (!SecsOrErr)
    fail(toString(SecsOrErr.takeError()));
  auto Secs = *SecsOrErr;

  // Allocated sections, in address order.
  SmallVector<unsigned> Alloc;
  for (unsigned I = 0; I < Secs.size(); ++I) {
    const Elf_Shdr &S = Secs[I];
    if (!(S.sh_flags & ELF::SHF_ALLOC) || S.sh_size == 0)
      continue;
    if (S.sh_flags & ELF::SHF_TLS)
      fail("thread-local data isn't supported on TOS");
    Alloc.push_back(I);
  }
  if (Alloc.empty())
    fail(InputFile + ": no code");
  llvm::stable_sort(Alloc, [&](unsigned A, unsigned B) {
    return Secs[A].sh_addr < Secs[B].sh_addr;
  });

  // Split the image into text, data and bss.
  uint64_t Base = Secs[Alloc.front()].sh_addr;
  uint64_t DataStart = 0, BssStart = 0, End = Base;
  bool InBss = false;
  for (unsigned I : Alloc) {
    const Elf_Shdr &S = Secs[I];
    Expected<StringRef> Name = Obj.getSectionName(S);
    if (!Name)
      fail(toString(Name.takeError()));
    if (S.sh_addr < End)
      fail("section " + *Name + " overlaps the one before it");
    if (S.sh_type == ELF::SHT_NOBITS) {
      if (!InBss)
        BssStart = End;
      InBss = true;
    } else {
      if (InBss)
        fail("section " + *Name +
             " has contents but comes after the bss; TOS programs keep all "
             "uninitialized data at the end");
      if (!DataStart && (S.sh_flags & ELF::SHF_WRITE))
        DataStart = S.sh_addr;
    }
    End = S.sh_addr + S.sh_size;
  }
  if (!InBss)
    BssStart = End;
  if (!DataStart)
    DataStart = BssStart;
  if (Hdr.e_entry != Base)
    fail("the entry point must be the first byte of the text; link the "
         "startup code first");
  if ((DataStart - Base) % 2 || (BssStart - Base) % 2)
    fail("the data and bss must start at even addresses");

  std::vector<uint8_t> Image(BssStart - Base, 0);
  for (unsigned I : Alloc) {
    const Elf_Shdr &S = Secs[I];
    if (S.sh_type == ELF::SHT_NOBITS)
      continue;
    auto Contents = Obj.getSectionContents(S);
    if (!Contents)
      fail(toString(Contents.takeError()));
    llvm::copy(*Contents, Image.begin() + (S.sh_addr - Base));
  }

  auto InImage = [&](unsigned Shndx) {
    return Shndx != ELF::SHN_UNDEF && Shndx != ELF::SHN_ABS &&
           Shndx < ELF::SHN_LORESERVE && Shndx < Secs.size() &&
           (Secs[Shndx].sh_flags & ELF::SHF_ALLOC);
  };

  // Turn absolute relocations into fixups.
  std::vector<uint32_t> Fixups;
  bool SawRelocs = false;
  for (const Elf_Shdr &RS : Secs) {
    if (RS.sh_type == ELF::SHT_REL)
      fail("SHT_REL relocations aren't supported");
    if (RS.sh_type != ELF::SHT_RELA || RS.sh_info >= Secs.size())
      continue;
    const Elf_Shdr &Target = Secs[RS.sh_info];
    if (!(Target.sh_flags & ELF::SHF_ALLOC))
      continue; // Relocations for a non-allocated section, such as debug info.
    SawRelocs = true;
    auto SymTab = Obj.getSection(RS.sh_link);
    if (!SymTab)
      fail(toString(SymTab.takeError()));
    auto Relas = Obj.relas(RS);
    if (!Relas)
      fail(toString(Relas.takeError()));
    for (const auto &R : *Relas) {
      uint32_t Type = R.getType(false);
      if (Type == ELF::R_68K_NONE)
        continue;
      // In an executable, r_offset is the address of the field.
      if (R.r_offset < Target.sh_addr ||
          R.r_offset >= Target.sh_addr + Target.sh_size)
        fail("relocation outside its section");
      auto Sym = Obj.getRelocationSymbol(R, *SymTab);
      if (!Sym)
        fail(toString(Sym.takeError()));
      unsigned Shndx = *Sym ? (*Sym)->st_shndx : ELF::SHN_UNDEF;
      // A section symbol of a section the linker dropped is index 0.
      bool Relocatable = InImage(Shndx);
      uint64_t Off = R.r_offset - Base;
      switch (Type) {
      case ELF::R_68K_32: {
        if (!Relocatable)
          continue; // Absolute or undefined weak: the value is final.
        if (Target.sh_type == ELF::SHT_NOBITS)
          fail("relocation in the bss");
        if (Off + 4 > Image.size())
          fail("relocation outside its section");
        if (Off % 2)
          fail("a 32-bit address at odd offset " + Twine(Off) +
               " can't be relocated by GEMDOS");
        uint32_t V = support::endian::read32be(&Image[Off]);
        support::endian::write32be(&Image[Off], uint32_t(V - Base));
        Fixups.push_back(Off);
        break;
      }
      case ELF::R_68K_PC32:
      case ELF::R_68K_PC16:
      case ELF::R_68K_PC8:
      case ELF::R_68K_PLT32:
      case ELF::R_68K_PLT16:
      case ELF::R_68K_PLT8:
        // The whole program moves together, so these stay right.
        break;
      case ELF::R_68K_16:
      case ELF::R_68K_8:
        if (Relocatable)
          fail("a 16- or 8-bit absolute address at offset " + Twine(Off) +
               " can't be relocated; build with the large code model "
               "(-mcmodel=large)");
        break;
      default:
        fail("unsupported relocation type " + Twine(Type));
      }
    }
  }
  if (!SawRelocs)
    WithColor::warning(errs(), ToolName)
        << "no relocations found; was the program linked with --emit-relocs?\n";
  llvm::sort(Fixups);
  Fixups.erase(std::unique(Fixups.begin(), Fixups.end()), Fixups.end());
  for (size_t I = 1; I < Fixups.size(); ++I)
    if (Fixups[I] - Fixups[I - 1] < 4)
      fail("overlapping relocations at offset " + Twine(Fixups[I]));

  // DRI symbol table: 8-character names, a type word and a value, which is
  // the offset from the start of the text.
  std::string SymOut;
  if (Symbols) {
    auto SymTabSec = llvm::find_if(
        Secs, [](const Elf_Shdr &S) { return S.sh_type == ELF::SHT_SYMTAB; });
    if (SymTabSec != Secs.end()) {
      auto Syms = Obj.symbols(&*SymTabSec);
      auto StrTab = Obj.getStringTableForSymtab(*SymTabSec);
      if (Syms && StrTab)
        for (const auto &Sym : *Syms) {
          unsigned T = Sym.getType();
          if (T != ELF::STT_FUNC && T != ELF::STT_OBJECT &&
              T != ELF::STT_NOTYPE)
            continue;
          if (!InImage(Sym.st_shndx))
            continue;
          auto Name = Sym.getName(*StrTab);
          if (!Name || Name->empty() || Name->starts_with(".L") ||
              Name->starts_with("$"))
            continue;
          uint64_t A = Sym.st_value;
          if (A < Base || A > End)
            continue;
          uint16_t Ty = 0x8000; // defined
          if (A < DataStart)
            Ty |= 0x0200; // text
          else if (A < BssStart)
            Ty |= 0x0400; // data
          else
            Ty |= 0x0100; // bss
          if (Sym.getBinding() != ELF::STB_LOCAL)
            Ty |= 0x2000; // global
          std::string N = Name->substr(0, 8).str();
          N.resize(8, '\0');
          SymOut += N;
          put16(SymOut, Ty);
          put32(SymOut, uint32_t(A - Base));
        }
    }
  }

  uint32_t TLen = DataStart - Base, DLen = BssStart - DataStart,
           BLen = End - BssStart;
  std::string Out;
  put16(Out, 0x601A);
  put32(Out, TLen);
  put32(Out, DLen);
  put32(Out, BLen);
  put32(Out, SymOut.size());
  put32(Out, 0); // reserved
  put32(Out, PrgFlags);
  put16(Out, 0); // relocation information follows
  Out.append(reinterpret_cast<const char *>(Image.data()), Image.size());
  Out += SymOut;

  // The fixup table: the offset of the first address as a long, then for
  // each next one the distance to it as a byte, with 1 meaning "254 bytes
  // further, nothing to fix here", and a 0 byte at the end.
  if (Fixups.empty()) {
    put32(Out, 0);
  } else {
    put32(Out, Fixups[0]);
    for (size_t I = 1; I < Fixups.size(); ++I) {
      uint32_t D = Fixups[I] - Fixups[I - 1];
      for (; D > 254; D -= 254)
        Out += char(1);
      Out += char(D);
    }
    Out += char(0);
  }
  if (Out.size() % 2)
    Out += char(0);

  if (Verbose)
    outs() << "text " << TLen << ", data " << DLen << ", bss " << BLen
           << " bytes, " << Fixups.size() << " relocations, "
           << SymOut.size() / 14 << " symbols\n";

  auto BufOut = FileOutputBuffer::create(OutputFile, Out.size(),
                                         FileOutputBuffer::F_executable);
  if (!BufOut)
    fail(OutputFile + ": " + toString(BufOut.takeError()));
  llvm::copy(Out, (*BufOut)->getBufferStart());
  if (Error E = (*BufOut)->commit())
    fail(OutputFile + ": " + toString(std::move(E)));
  return 0;
}
