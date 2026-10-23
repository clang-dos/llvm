//===- lib/MC/OMFObjectWriter.cpp - OMF File Writer -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the 16-bit OMF object writer used for DOS.
//
// MC builds ELF-style sections. Each allocatable section is placed in one of
// the classic segments below, in this order; several sections may share a
// segment and are then laid out one after another:
//
//   _TEXT  class CODE   executable sections
//   CONST  class DATA   read-only data
//   _DATA  class DATA   writable data
//   _BSS   class BSS    zero-initialized data (no LEDATA is written)
//   STACK  class STACK  sections named .stack, combine type stack
//
// DGROUP holds CONST, _DATA, _BSS and STACK, and _TEXT as well for the tiny
// memory model. Non-allocatable sections (debug info, notes) and unwind
// tables are not written.
//
// Fixups are written with no displacement field. As other OMF tools do, the
// displacement goes in the LEDATA bytes at the fixup location: for a target
// in this module that is its offset in its segment plus the addend.
//
// A defined global symbol named "_start" becomes the module's start address.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/BinaryFormat/OMF.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCOMFObjectWriter.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

MCOMFObjectTargetWriter::~MCOMFObjectTargetWriter() = default;

namespace {

enum SegmentKind { SK_Text, SK_Const, SK_Data, SK_Bss, SK_Stack, SK_Count };

struct SegmentInfo {
  const char *Name;
  const char *Class;
  uint8_t Combine;
};

const SegmentInfo Segments[SK_Count] = {
    {"_TEXT", "CODE", OMF::SC_Public}, {"CONST", "DATA", OMF::SC_Public},
    {"_DATA", "DATA", OMF::SC_Public}, {"_BSS", "BSS", OMF::SC_Public},
    {"STACK", "STACK", OMF::SC_Stack},
};

struct Segment {
  bool Present = false;
  unsigned Index = 0; // SEGDEF index, from 1.
  uint64_t Size = 0;
  Align MaxAlign = Align(1);
  SmallVector<const MCSection *, 4> Sections;
};

struct OMFFixup {
  SegmentKind Seg;  // Segment holding the fixup location.
  uint64_t Offset;  // Location offset within that segment.
  uint8_t LocType;  // OMF::LocationType.
  bool SegRelative; // M bit; false means self-relative.
  // Target: a segment of this module, or an external index.
  bool IsExternal;
  SegmentKind TargetSeg;
  unsigned ExtIndex;
};

/// One OMF record, assembled in memory and then written with its length and
/// checksum.
class Record {
  uint8_t Type;
  SmallVector<uint8_t, 64> Data;

public:
  explicit Record(uint8_t Type) : Type(Type) {}

  size_t size() const { return Data.size(); }
  bool empty() const { return Data.empty(); }
  void clear() { Data.clear(); }

  void byte(uint8_t B) { Data.push_back(B); }
  void word(uint16_t W) {
    byte(W & 0xff);
    byte(W >> 8);
  }
  void index(unsigned I) {
    assert(I < 0x8000 && "OMF index out of range");
    if (I < 0x80) {
      byte(I);
    } else {
      byte(0x80 | (I >> 8));
      byte(I & 0xff);
    }
  }
  void name(StringRef S) {
    byte(S.size());
    Data.append(S.bytes_begin(), S.bytes_end());
  }
  void bytes(ArrayRef<uint8_t> B) { Data.append(B.begin(), B.end()); }

  void write(raw_ostream &OS) const {
    uint8_t Header[3] = {Type, uint8_t((Data.size() + 1) & 0xff),
                         uint8_t((Data.size() + 1) >> 8)};
    uint8_t Sum = Header[0] + Header[1] + Header[2];
    for (uint8_t B : Data)
      Sum += B;
    OS.write(reinterpret_cast<const char *>(Header), 3);
    OS.write(reinterpret_cast<const char *>(Data.data()), Data.size());
    OS << char(uint8_t(-Sum));
  }
};

class OMFObjectWriter final : public MCObjectWriter {
  std::unique_ptr<MCOMFObjectTargetWriter> TargetObjectWriter;
  raw_pwrite_stream &OS;

  Segment Segs[SK_Count];
  // Placement of each written section: its segment and offset in it.
  DenseMap<const MCSection *, std::pair<SegmentKind, uint64_t>> SectionMap;
  // External indices for EXTDEF and COMDEF, which share one index space.
  DenseMap<const MCSymbol *, unsigned> ExtIndices;
  SmallVector<const MCSymbol *, 16> Externals;
  std::vector<OMFFixup> Fixups;
  unsigned DGroupIndex = 0;

  bool inDGROUP(SegmentKind K) const {
    return K != SK_Text || TargetObjectWriter->isCodeInDGROUP();
  }
  std::optional<SegmentKind> classifySection(const MCSectionELF &Sec) const;
  unsigned getExternalIndex(const MCSymbol &Sym);
  bool getSymbolLocation(const MCSymbol &Sym, SegmentKind &Seg,
                         uint64_t &Offset) const;

  void writeHeader();
  void writeNamesAndSegments();
  void writeExternals();
  void writePublics();
  void writeSegmentData(SegmentKind K);
  void writeFixups(SegmentKind K, uint64_t Start, uint64_t End,
                   ArrayRef<const OMFFixup *> SegFixups);
  void writeModuleEnd();

public:
  OMFObjectWriter(std::unique_ptr<MCOMFObjectTargetWriter> MOTW,
                  raw_pwrite_stream &OS)
      : TargetObjectWriter(std::move(MOTW)), OS(OS) {}

  void reset() override {
    for (Segment &S : Segs)
      S = Segment();
    SectionMap.clear();
    ExtIndices.clear();
    Externals.clear();
    Fixups.clear();
    DGroupIndex = 0;
    MCObjectWriter::reset();
  }

  void setAssembler(MCAssembler *A) override {
    MCObjectWriter::setAssembler(A);
    TargetObjectWriter->setAssembler(A);
  }

  void executePostLayoutBinding() override;
  void recordRelocation(const MCFragment &F, const MCFixup &Fixup,
                        MCValue Target, uint64_t &FixedValue) override;
  uint64_t writeObject() override;
};

} // end anonymous namespace

std::optional<SegmentKind>
OMFObjectWriter::classifySection(const MCSectionELF &Sec) const {
  unsigned Flags = Sec.getFlags();
  StringRef Name = Sec.getName();
  if (!(Flags & ELF::SHF_ALLOC) || (Flags & ELF::SHF_EXCLUDE))
    return std::nullopt;
  if (Name.starts_with(".eh_frame") || Name.starts_with(".note") ||
      Name.starts_with(".llvm"))
    return std::nullopt;
  if (Name == ".stack" || Name.starts_with(".stack."))
    return SK_Stack;
  if (Flags & ELF::SHF_EXECINSTR)
    return SK_Text;
  if (Sec.getType() == ELF::SHT_NOBITS)
    return SK_Bss;
  if (Flags & ELF::SHF_WRITE)
    return SK_Data;
  return SK_Const;
}

void OMFObjectWriter::executePostLayoutBinding() {
  // Place sections in segments.
  for (const MCSection &S : *Asm) {
    const auto &Sec = static_cast<const MCSectionELF &>(S);
    std::optional<SegmentKind> K = classifySection(Sec);
    if (!K)
      continue;
    Segment &Seg = Segs[*K];
    Seg.Present = true;
    Seg.Size = alignTo(Seg.Size, Sec.getAlign());
    Seg.MaxAlign = std::max(Seg.MaxAlign, Sec.getAlign());
    SectionMap[&Sec] = {*K, Seg.Size};
    Seg.Sections.push_back(&Sec);
    Seg.Size += Asm->getSectionAddressSize(Sec);
  }

  unsigned Index = 0;
  bool AnyInGroup = false;
  for (unsigned K = 0; K != SK_Count; ++K) {
    if (!Segs[K].Present)
      continue;
    Segs[K].Index = ++Index;
    AnyInGroup |= inDGROUP(SegmentKind(K));
    if (Segs[K].Size > 0x10000)
      getContext().reportError(SMLoc(), Twine("segment ") + Segments[K].Name +
                                            " is larger than 64 KiB");
  }
  // Communal variables are near data and belong to DGROUP too.
  for (const MCSymbol &Sym : Asm->symbols())
    if (Sym.isCommon())
      AnyInGroup = true;
  DGroupIndex = AnyInGroup ? 1 : 0;

  // Communal variables come first in the external index space.
  for (const MCSymbol &Sym : Asm->symbols())
    if (Sym.isCommon() && !ExtIndices.count(&Sym)) {
      Externals.push_back(&Sym);
      ExtIndices[&Sym] = Externals.size();
    }
}

unsigned OMFObjectWriter::getExternalIndex(const MCSymbol &Sym) {
  auto [It, Inserted] = ExtIndices.try_emplace(&Sym, Externals.size() + 1);
  if (Inserted)
    Externals.push_back(&Sym);
  return It->second;
}

bool OMFObjectWriter::getSymbolLocation(const MCSymbol &Sym, SegmentKind &Seg,
                                        uint64_t &Offset) const {
  const MCSymbol *Base = Asm->getBaseSymbol(Sym);
  if (!Base || !Base->isInSection())
    return false;
  auto It = SectionMap.find(&Base->getSection());
  if (It == SectionMap.end())
    return false;
  uint64_t SymOffset;
  if (!Asm->getSymbolOffset(Sym, SymOffset))
    return false;
  Seg = It->second.first;
  Offset = It->second.second + SymOffset;
  return true;
}

void OMFObjectWriter::recordRelocation(const MCFragment &F,
                                       const MCFixup &Fixup, MCValue Target,
                                       uint64_t &FixedValue) {
  auto SecIt = SectionMap.find(F.getParent());
  // Fixups in sections that are not written (debug info) are dropped.
  if (SecIt == SectionMap.end()) {
    FixedValue = 0;
    return;
  }
  MCContext &Ctx = getContext();
  SMLoc Loc = Fixup.getLoc();

  if (Target.getSubSym() || Target.getSpecifier()) {
    Ctx.reportError(Loc, "unsupported relocation expression in OMF object");
    return;
  }
  const MCSymbol *Sym = Target.getAddSym();
  if (!Sym) {
    Ctx.reportError(Loc, "unsupported absolute PC-relative fixup in OMF");
    return;
  }

  bool PCRel = Fixup.isPCRel();
  switch (Fixup.getKind()) {
  case FK_Data_2:
    break;
  default:
    Ctx.reportError(Loc, "unsupported relocation size in OMF object; only "
                         "16-bit offsets are supported");
    return;
  }

  OMFFixup R;
  R.Seg = SecIt->second.first;
  R.Offset =
      SecIt->second.second + Asm->getFragmentOffset(F) + Fixup.getOffset();
  R.LocType = OMF::LT_Offset16;
  R.SegRelative = !PCRel;

  int64_t Addend = Target.getConstant();
  // The X86 backend has already biased PC-relative constants by the field
  // size; the self-relative OMF fixup does that itself.
  if (PCRel)
    Addend += 2;

  SegmentKind TargetSeg;
  uint64_t TargetOffset;
  if (!Sym->isCommon() && getSymbolLocation(*Sym, TargetSeg, TargetOffset)) {
    R.IsExternal = false;
    R.TargetSeg = TargetSeg;
    R.ExtIndex = 0;
    Addend += TargetOffset;
  } else if (Sym->isUndefined() || Sym->isCommon()) {
    R.IsExternal = true;
    R.TargetSeg = SK_Text;
    R.ExtIndex = getExternalIndex(*Sym);
  } else {
    Ctx.reportError(Loc, "symbol '" + Sym->getName() +
                             "' is not in a section written to the OMF "
                             "object");
    return;
  }

  FixedValue = uint64_t(Addend) & 0xffff;
  Fixups.push_back(R);
}

void OMFObjectWriter::writeHeader() {
  std::string Name = "noname";
  if (!getFileNames().empty())
    Name = sys::path::filename(getFileNames().front().first).str();
  if (Name.size() > 255)
    Name.resize(255);
  Record THeadr(OMF::THEADR);
  THeadr.name(Name);
  THeadr.write(OS);

  Record Coment(OMF::COMENT);
  Coment.byte(0x00); // Attributes.
  Coment.byte(OMF::CC_Translator);
  StringRef Translator = "LLVM";
  Coment.byte(Translator.size());
  Coment.bytes(ArrayRef(Translator.bytes_begin(), Translator.bytes_end()));
  Coment.write(OS);
}

void OMFObjectWriter::writeNamesAndSegments() {
  // LNAMES: 1 is the empty name, then DGROUP (if used), then the segment and
  // class names of the present segments.
  SmallVector<StringRef, 16> Names;
  auto NameIndex = [&](StringRef N) -> unsigned {
    for (unsigned I = 0, E = Names.size(); I != E; ++I)
      if (Names[I] == N)
        return I + 1;
    Names.push_back(N);
    return Names.size();
  };
  NameIndex("");
  unsigned DGroupName = DGroupIndex ? NameIndex("DGROUP") : 0;
  unsigned SegName[SK_Count] = {}, ClassName[SK_Count] = {};
  for (unsigned K = 0; K != SK_Count; ++K) {
    if (!Segs[K].Present)
      continue;
    SegName[K] = NameIndex(Segments[K].Name);
    ClassName[K] = NameIndex(Segments[K].Class);
  }
  Record LNames(OMF::LNAMES);
  for (StringRef N : Names)
    LNames.name(N);
  LNames.write(OS);

  for (unsigned K = 0; K != SK_Count; ++K) {
    const Segment &Seg = Segs[K];
    if (!Seg.Present)
      continue;
    uint8_t A = Seg.MaxAlign.value() == 1    ? OMF::SA_Byte
                : Seg.MaxAlign.value() == 2  ? OMF::SA_Word
                : Seg.MaxAlign.value() <= 16 ? OMF::SA_Para
                                             : OMF::SA_Page;
    bool Big = Seg.Size == 0x10000;
    Record SegDef(OMF::SEGDEF);
    SegDef.byte((A << 5) | (Segments[K].Combine << 2) | (Big << 1));
    SegDef.word(Big ? 0 : Seg.Size);
    SegDef.index(SegName[K]);
    SegDef.index(ClassName[K]);
    SegDef.index(1); // Overlay name: the empty name.
    SegDef.write(OS);
  }

  if (DGroupIndex) {
    Record GrpDef(OMF::GRPDEF);
    GrpDef.index(DGroupName);
    for (unsigned K = 0; K != SK_Count; ++K) {
      if (!Segs[K].Present || !inDGROUP(SegmentKind(K)))
        continue;
      GrpDef.byte(0xff);
      GrpDef.index(Segs[K].Index);
    }
    GrpDef.write(OS);
  }
}

void OMFObjectWriter::writeExternals() {
  // Externals are numbered in the order of the COMDEF and EXTDEF records.
  // Communal variables were numbered first, so their COMDEF records come
  // first too.
  Record ComDef(OMF::COMDEF), ExtDef(OMF::EXTDEF);
  auto Flush = [&](Record &R) {
    if (!R.empty())
      R.write(OS);
    R.clear();
  };
  for (const MCSymbol *Sym : Externals) {
    StringRef Name = Sym->getName();
    if (Name.size() > 255) {
      getContext().reportError(SMLoc(), "symbol name '" + Name +
                                            "' is too long for OMF");
      Name = Name.take_front(255);
    }
    if (Sym->isCommon()) {
      if (ComDef.size() + Name.size() + 8 > OMF::MaxRecordData)
        Flush(ComDef);
      ComDef.name(Name);
      ComDef.index(0); // Type index.
      ComDef.byte(OMF::CT_Near);
      uint64_t Size = Sym->getCommonSize();
      if (Size < 0x80) {
        ComDef.byte(Size);
      } else if (Size <= 0xffff) {
        ComDef.byte(0x81);
        ComDef.word(Size);
      } else {
        ComDef.byte(0x84);
        ComDef.word(Size & 0xffff);
        ComDef.byte(Size >> 16);
      }
    } else {
      // Keep the numbering in order: a COMDEF never follows an EXTDEF.
      Flush(ComDef);
      if (ExtDef.size() + Name.size() + 2 > OMF::MaxRecordData)
        Flush(ExtDef);
      ExtDef.name(Name);
      ExtDef.index(0); // Type index.
    }
  }
  Flush(ComDef);
  Flush(ExtDef);
}

void OMFObjectWriter::writePublics() {
  // Group publics by segment. Absolute symbols use segment 0.
  SmallVector<std::pair<StringRef, uint64_t>, 16> Publics[SK_Count + 1];
  for (const MCSymbol &S : Asm->symbols()) {
    const auto &Sym = static_cast<const MCSymbolELF &>(S);
    if (Sym.isTemporary() || Sym.isCommon() || !Sym.isBindingSet() ||
        Sym.getBinding() == ELF::STB_LOCAL)
      continue;
    if (Sym.isInSection() && Sym.getSection().getBeginSymbol() == &Sym)
      continue;
    SegmentKind K;
    uint64_t Offset;
    if (getSymbolLocation(Sym, K, Offset)) {
      Publics[K].push_back({Sym.getName(), Offset});
      continue;
    }
    if (Sym.isVariable()) {
      int64_t Value;
      if (Sym.getVariableValue()->evaluateAsAbsolute(Value, *Asm)) {
        Publics[SK_Count].push_back({Sym.getName(), uint64_t(Value)});
        continue;
      }
    }
    if (!Sym.isUndefined())
      getContext().reportError(SMLoc(), "symbol '" + Sym.getName() +
                                            "' cannot be exported to OMF");
  }

  for (unsigned K = 0; K <= SK_Count; ++K) {
    if (Publics[K].empty())
      continue;
    Record PubDef(OMF::PUBDEF);
    auto Start = [&] {
      if (K == SK_Count) {
        PubDef.index(0);
        PubDef.index(0);
        PubDef.word(0); // Frame number.
      } else {
        PubDef.index(inDGROUP(SegmentKind(K)) ? DGroupIndex : 0);
        PubDef.index(Segs[K].Index);
      }
    };
    Start();
    size_t HeaderSize = PubDef.size();
    for (auto &[Name, Offset] : Publics[K]) {
      StringRef N = Name.take_front(255);
      if (PubDef.size() + N.size() + 4 > OMF::MaxRecordData) {
        PubDef.write(OS);
        PubDef.clear();
        Start();
      }
      PubDef.name(N);
      PubDef.word(Offset);
      PubDef.index(0); // Type index.
    }
    if (PubDef.size() > HeaderSize)
      PubDef.write(OS);
  }
}

void OMFObjectWriter::writeFixups(SegmentKind K, uint64_t Start, uint64_t End,
                                  ArrayRef<const OMFFixup *> SegFixups) {
  Record FixUpp(OMF::FIXUPP);
  for (const OMFFixup *R : SegFixups) {
    if (R->Offset < Start || R->Offset >= End)
      continue;
    unsigned DataOffset = R->Offset - Start;
    FixUpp.byte(0x80 | (R->SegRelative << 6) | (R->LocType << 2) |
                (DataOffset >> 8));
    FixUpp.byte(DataOffset & 0xff);

    // Use DGROUP as the frame for targets in DGROUP, so that data offsets
    // are relative to DS. Otherwise the frame is the target's own.
    bool GroupFrame = R->IsExternal ? TargetObjectWriter->isCodeInDGROUP()
                                    : inDGROUP(R->TargetSeg);
    uint8_t Frame = GroupFrame ? OMF::F_Group : OMF::F_Target;
    uint8_t Method =
        R->IsExternal ? OMF::T_ExternalNoDisp : OMF::T_SegmentNoDisp;
    FixUpp.byte((Frame << 4) | Method);
    if (GroupFrame)
      FixUpp.index(DGroupIndex);
    FixUpp.index(R->IsExternal ? R->ExtIndex : Segs[R->TargetSeg].Index);
  }
  if (!FixUpp.empty())
    FixUpp.write(OS);
}

void OMFObjectWriter::writeSegmentData(SegmentKind K) {
  const Segment &Seg = Segs[K];
  if (!Seg.Present || Seg.Size == 0)
    return;

  // Build the segment image. Bytes of virtual (bss) sections stay zero.
  SmallVector<uint8_t, 0> Image(Seg.Size, 0);
  bool AnyData = false;
  uint64_t PrevEnd = 0;
  for (const MCSection *Sec : Seg.Sections) {
    uint64_t Base = SectionMap.lookup(Sec).second;
    // Code padding between sections is NOPs.
    if (K == SK_Text)
      std::fill(Image.begin() + PrevEnd, Image.begin() + Base, 0x90);
    PrevEnd = Base + Asm->getSectionAddressSize(*Sec);
    if (Sec->isBssSection())
      continue;
    SmallString<256> Buf;
    raw_svector_ostream VOS(Buf);
    Asm->writeSectionData(VOS, Sec);
    std::copy(Buf.begin(), Buf.end(), Image.begin() + Base);
    AnyData = true;
  }
  if (!AnyData)
    return;

  SmallVector<const OMFFixup *, 32> SegFixups;
  std::vector<bool> InFixup(Seg.Size, false);
  for (const OMFFixup &R : Fixups) {
    if (R.Seg != K)
      continue;
    SegFixups.push_back(&R);
    InFixup[R.Offset] = InFixup[R.Offset + 1] = true;
  }

  // Runs of at least this many zero bytes with no fixup are not written;
  // linkers zero-fill them.
  constexpr uint64_t MinZeroRun = 16;
  auto ZeroRunAt = [&](uint64_t I) {
    uint64_t J = I;
    while (J < Seg.Size && Image[J] == 0 && !InFixup[J])
      ++J;
    return J - I;
  };

  // LEDATA payload: segment index (1 byte for < 128) and a 2-byte offset.
  const uint64_t MaxChunk = OMF::MaxRecordData - 4;
  uint64_t I = 0;
  while (I < Seg.Size) {
    uint64_t Zeros = ZeroRunAt(I);
    if (Zeros >= MinZeroRun) {
      I += Zeros;
      continue;
    }
    uint64_t End = I;
    while (End < Seg.Size && End - I < MaxChunk) {
      if (Image[End] == 0 && !InFixup[End]) {
        uint64_t Z = ZeroRunAt(End);
        if (Z >= MinZeroRun)
          break;
        End = std::min(End + Z, I + MaxChunk);
        continue;
      }
      ++End;
    }
    // Do not split a fixup field across two records.
    for (const OMFFixup *R : SegFixups)
      if (R->Offset < End && R->Offset + 2 > End)
        End = R->Offset;

    Record LEData(OMF::LEDATA);
    LEData.index(Seg.Index);
    LEData.word(I);
    LEData.bytes(ArrayRef(Image).slice(I, End - I));
    LEData.write(OS);
    writeFixups(K, I, End, SegFixups);
    I = End;
  }
}

void OMFObjectWriter::writeModuleEnd() {
  Record ModEnd(OMF::MODEND);
  const MCSymbol *Start = nullptr;
  for (const MCSymbol &S : Asm->symbols()) {
    const auto &Sym = static_cast<const MCSymbolELF &>(S);
    if (Sym.getName() == "_start" && Sym.isBindingSet() &&
        Sym.getBinding() != ELF::STB_LOCAL && !Sym.isUndefined())
      Start = &Sym;
  }
  SegmentKind K;
  uint64_t Offset;
  if (Start && getSymbolLocation(*Start, K, Offset)) {
    // Main module with a start address: frame and target given explicitly,
    // with a displacement.
    ModEnd.byte(0xc1);
    if (inDGROUP(K)) {
      ModEnd.byte((OMF::F_Group << 4) | OMF::T_Segment);
      ModEnd.index(DGroupIndex);
    } else {
      ModEnd.byte((OMF::F_Segment << 4) | OMF::T_Segment);
      ModEnd.index(Segs[K].Index);
    }
    ModEnd.index(Segs[K].Index);
    ModEnd.word(Offset);
  } else {
    ModEnd.byte(0x00);
  }
  ModEnd.write(OS);
}

uint64_t OMFObjectWriter::writeObject() {
  uint64_t StartOffset = OS.tell();
  writeHeader();
  writeNamesAndSegments();
  writeExternals();
  writePublics();
  for (unsigned K = 0; K != SK_Count; ++K)
    writeSegmentData(SegmentKind(K));
  writeModuleEnd();
  return OS.tell() - StartOffset;
}

std::unique_ptr<MCObjectWriter>
llvm::createOMFObjectWriter(std::unique_ptr<MCOMFObjectTargetWriter> MOTW,
                            raw_pwrite_stream &OS) {
  return std::make_unique<OMFObjectWriter>(std::move(MOTW), OS);
}
