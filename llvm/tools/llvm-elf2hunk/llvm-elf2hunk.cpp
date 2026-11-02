//===- llvm-elf2hunk.cpp - Convert an m68k ELF program to an Amiga Hunk ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Turns an m68k ELF executable, linked with --emit-relocs, into an AmigaOS
// Hunk executable that LoadSeg() can load.
//
// Each run of allocated sections of the same kind (code, data or bss, in
// chip or any memory) becomes one hunk. LoadSeg() puts every hunk at its own
// address, so each 32-bit absolute relocation becomes a HUNK_RELOC32 entry
// relative to the start of its target hunk. PC-relative references must stay
// inside one hunk. The program starts at the first byte of the first hunk,
// so the entry point has to be there.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <map>

using namespace llvm;
using namespace llvm::object;

static cl::OptionCategory Cat("llvm-elf2hunk options");
static cl::opt<std::string> InputFile(cl::Positional, cl::Required,
                                      cl::desc("<input ELF>"), cl::cat(Cat));
static cl::opt<std::string> OutputFile("o", cl::Required,
                                       cl::desc("Output Hunk file"),
                                       cl::value_desc("file"), cl::cat(Cat));
static cl::opt<bool> Symbols("symbols",
                             cl::desc("Write HUNK_SYMBOL tables for debuggers"),
                             cl::cat(Cat));
static cl::opt<bool> Verbose("v", cl::desc("Print the hunk layout"),
                             cl::cat(Cat));

enum : uint32_t {
  HUNK_CODE = 0x3E9,
  HUNK_DATA = 0x3EA,
  HUNK_BSS = 0x3EB,
  HUNK_RELOC32 = 0x3EC,
  HUNK_SYMBOL = 0x3F0,
  HUNK_END = 0x3F2,
  HUNK_HEADER = 0x3F3,
  HUNKF_CHIP = 1u << 30,
};

static StringRef ToolName;

[[noreturn]] static void fail(const Twine &Msg) {
  WithColor::error(errs(), ToolName) << Msg << "\n";
  exit(1);
}

namespace {
enum class Kind { Code, Data, Bss };

struct Hunk {
  Kind K;
  bool Chip = false;
  uint64_t Base = 0, End = 0;
  std::vector<uint8_t> Bytes;                       // empty for bss
  std::map<uint32_t, std::vector<uint32_t>> Relocs; // target hunk -> offsets
  std::vector<std::pair<std::string, uint32_t>> Syms;
};
} // namespace

using ELFT = ELF32BE;
using Elf_Shdr = ELFT::Shdr;

static Kind kindOf(const Elf_Shdr &S) {
  if (S.sh_type == ELF::SHT_NOBITS)
    return Kind::Bss;
  if (S.sh_flags & ELF::SHF_EXECINSTR)
    return Kind::Code;
  return Kind::Data;
}

// gcc for AmigaOS puts __chip variables in sections such as .datachip and
// .bsschip; accept .data_chip and .bss_chip too.
static bool isChip(StringRef Name) { return Name.contains("chip"); }

static void writeBE(raw_ostream &OS, uint32_t V) {
  char B[4];
  support::endian::write32be(B, V);
  OS.write(B, 4);
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);
  ToolName = argv[0];
  cl::HideUnrelatedOptions(Cat);
  cl::ParseCommandLineOptions(
      argc, argv,
      "Convert an m68k ELF program to an AmigaOS Hunk executable\n");

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
      fail("thread-local data isn't supported on AmigaOS");
    Alloc.push_back(I);
  }
  llvm::stable_sort(Alloc, [&](unsigned A, unsigned B) {
    return Secs[A].sh_addr < Secs[B].sh_addr;
  });

  // Group runs of sections of one kind into hunks.
  std::vector<Hunk> Hunks;
  DenseMap<unsigned, unsigned> SecToHunk;
  for (unsigned I : Alloc) {
    const Elf_Shdr &S = Secs[I];
    Expected<StringRef> Name = Obj.getSectionName(S);
    if (!Name)
      fail(toString(Name.takeError()));
    Kind K = kindOf(S);
    bool Chip = isChip(*Name);
    if (Hunks.empty() || Hunks.back().K != K || Hunks.back().Chip != Chip) {
      Hunks.emplace_back();
      Hunks.back().K = K;
      Hunks.back().Chip = Chip;
      Hunks.back().Base = Hunks.back().End = S.sh_addr;
    }
    Hunk &H = Hunks.back();
    H.End = std::max<uint64_t>(H.End, S.sh_addr + S.sh_size);
    if (K != Kind::Bss) {
      H.Bytes.resize(H.End - H.Base, 0);
      auto Contents = Obj.getSectionContents(S);
      if (!Contents)
        fail(toString(Contents.takeError()));
      llvm::copy(*Contents, H.Bytes.begin() + (S.sh_addr - H.Base));
    }
    SecToHunk[I] = Hunks.size() - 1;
  }
  // LoadSeg() starts the program at the first byte of the first hunk, so
  // move the hunk that holds the entry point to the front.
  unsigned EntryHunk = Hunks.size();
  for (unsigned I = 0; I < Hunks.size(); ++I)
    if (Hunks[I].K == Kind::Code && Hdr.e_entry >= Hunks[I].Base &&
        Hdr.e_entry < Hunks[I].End)
      EntryHunk = I;
  if (EntryHunk == Hunks.size())
    fail("the entry point isn't in a code section");
  if (Hdr.e_entry != Hunks[EntryHunk].Base)
    fail("the entry point must be the first byte of the code; link the "
         "startup code first");
  if (EntryHunk != 0) {
    std::rotate(Hunks.begin(), Hunks.begin() + EntryHunk,
                Hunks.begin() + EntryHunk + 1);
    for (auto &KV : SecToHunk) {
      if (KV.second == EntryHunk)
        KV.second = 0;
      else if (KV.second < EntryHunk)
        ++KV.second;
    }
  }

  auto HunkOfAddr = [&](uint64_t A) -> int {
    for (unsigned I = 0; I < Hunks.size(); ++I)
      if (A >= Hunks[I].Base && A < Hunks[I].End)
        return I;
    return -1;
  };

  // Turn relocations into HUNK_RELOC32 entries.
  bool SawRelocs = false;
  for (const Elf_Shdr &RS : Secs) {
    if (RS.sh_type == ELF::SHT_REL)
      fail("SHT_REL relocations aren't supported");
    if (RS.sh_type != ELF::SHT_RELA)
      continue;
    auto Src = SecToHunk.find(RS.sh_info);
    if (Src == SecToHunk.end())
      continue; // Relocations for a non-allocated section, such as debug info.
    SawRelocs = true;
    Hunk &SrcH = Hunks[Src->second];
    const Elf_Shdr &Target = Secs[RS.sh_info];
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
      uint64_t Off = R.r_offset - SrcH.Base;
      auto Sym = Obj.getRelocationSymbol(R, *SymTab);
      if (!Sym)
        fail(toString(Sym.takeError()));
      int64_t S = *Sym ? (*Sym)->st_value : 0;
      uint64_t V = S + R.r_addend;
      unsigned Shndx = *Sym ? (*Sym)->st_shndx : ELF::SHN_UNDEF;
      int TgtHunk = -1;
      if (Shndx != ELF::SHN_UNDEF && Shndx != ELF::SHN_ABS &&
          Shndx < ELF::SHN_LORESERVE) {
        auto It = SecToHunk.find(Shndx);
        TgtHunk = It != SecToHunk.end() ? (int)It->second : HunkOfAddr(S);
      }

      switch (Type) {
      case ELF::R_68K_32: {
        if (TgtHunk < 0)
          continue; // Absolute or undefined weak: the value is final.
        if (Off + 4 > SrcH.Bytes.size())
          fail("relocation outside its section");
        support::endian::write32be(&SrcH.Bytes[Off],
                                   uint32_t(V - Hunks[TgtHunk].Base));
        SrcH.Relocs[TgtHunk].push_back(Off);
        break;
      }
      case ELF::R_68K_PC32:
      case ELF::R_68K_PC16:
      case ELF::R_68K_PC8:
      case ELF::R_68K_PLT32:
      case ELF::R_68K_PLT16:
      case ELF::R_68K_PLT8:
        if (TgtHunk >= 0 && &Hunks[TgtHunk] != &SrcH)
          fail(Twine("a PC-relative reference at offset ") + Twine(Off) +
               " of hunk " + Twine(Src->second) + " goes to hunk " +
               Twine(TgtHunk) +
               "; build with the large code model (-mcmodel=large)");
        break;
      case ELF::R_68K_16:
      case ELF::R_68K_8:
        if (TgtHunk >= 0)
          fail("a 16- or 8-bit absolute reference to a hunk can't be "
               "relocated");
        break;
      default:
        fail("unsupported relocation type " + Twine(Type));
      }
    }
  }
  if (!SawRelocs)
    WithColor::warning(errs(), ToolName)
        << "no relocations found; was the program linked with --emit-relocs?\n";

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
          auto It = SecToHunk.find(Sym.st_shndx);
          if (It == SecToHunk.end() || Sym.st_shndx == ELF::SHN_UNDEF)
            continue;
          auto Name = Sym.getName(*StrTab);
          if (!Name || Name->empty() || Name->starts_with(".L") ||
              Name->starts_with("$"))
            continue;
          Hunk &H = Hunks[It->second];
          H.Syms.push_back({Name->str(), uint32_t(Sym.st_value - H.Base)});
        }
    }
  }

  // Write the file.
  std::string Out;
  raw_string_ostream OS(Out);
  writeBE(OS, HUNK_HEADER);
  writeBE(OS, 0); // no resident library names
  writeBE(OS, Hunks.size());
  writeBE(OS, 0);
  writeBE(OS, Hunks.size() - 1);
  for (const Hunk &H : Hunks)
    writeBE(OS, uint32_t((H.End - H.Base + 3) / 4) | (H.Chip ? HUNKF_CHIP : 0));

  for (unsigned I = 0; I < Hunks.size(); ++I) {
    Hunk &H = Hunks[I];
    uint32_t Longs = (H.End - H.Base + 3) / 4;
    if (Verbose)
      outs() << "hunk " << I << ": "
             << (H.K == Kind::Code   ? "CODE"
                 : H.K == Kind::Data ? "DATA"
                                     : "BSS")
             << (H.Chip ? " (chip)" : "") << ", " << (H.End - H.Base)
             << " bytes, " <<
          [&] {
            size_t N = 0;
            for (auto &R : H.Relocs)
              N += R.second.size();
            return N;
          }() << " relocations\n";
    if (H.K == Kind::Bss) {
      writeBE(OS, HUNK_BSS);
      writeBE(OS, Longs);
    } else {
      writeBE(OS, H.K == Kind::Code ? HUNK_CODE : HUNK_DATA);
      writeBE(OS, Longs);
      H.Bytes.resize(Longs * 4, 0);
      OS.write(reinterpret_cast<const char *>(H.Bytes.data()), H.Bytes.size());
    }
    if (!H.Relocs.empty()) {
      writeBE(OS, HUNK_RELOC32);
      for (auto &[Tgt, Offs] : H.Relocs) {
        llvm::sort(Offs);
        writeBE(OS, Offs.size());
        writeBE(OS, Tgt);
        for (uint32_t O : Offs)
          writeBE(OS, O);
      }
      writeBE(OS, 0);
    }
    if (!H.Syms.empty()) {
      writeBE(OS, HUNK_SYMBOL);
      for (auto &[Name, Val] : H.Syms) {
        uint32_t NL = (Name.size() + 3) / 4;
        writeBE(OS, NL);
        OS << Name;
        OS.write_zeros(NL * 4 - Name.size());
        writeBE(OS, Val);
      }
      writeBE(OS, 0);
    }
    writeBE(OS, HUNK_END);
  }

  auto BufOut = FileOutputBuffer::create(OutputFile, Out.size(),
                                         FileOutputBuffer::F_executable);
  if (!BufOut)
    fail(OutputFile + ": " + toString(BufOut.takeError()));
  llvm::copy(Out, (*BufOut)->getBufferStart());
  if (Error E = (*BufOut)->commit())
    fail(OutputFile + ": " + toString(std::move(E)));
  return 0;
}
