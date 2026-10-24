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
//   XIB, XI, XIE        constructors: .init_array, .preinit_array, .ctors
//   YIB, YI, YIE        destructors: .fini_array, .dtors
//   _DATA  class DATA   writable data
//   _BSS   class BSS    zero-initialized data (no LEDATA is written)
//   STACK  class STACK  sections named .stack, combine type stack
//
// Every module defines _TEXT, CONST, _DATA and _BSS, even when empty, so
// that linkers keep this order whatever module they see first.
//
// The constructor and destructor tables are arrays of near function pointers
// in XI and YI (class DATA), between the empty XIB/XIE and YIB/YIE segments,
// which the startup code defines (by naming sections XIB and so on) to label
// the start and end of each table. A module that uses any of the six defines
// them all, to keep them in order. Priorities are not kept: entries are in
// link order.
//
// DGROUP holds CONST, _DATA, _BSS and STACK, and _TEXT as well for the tiny
// memory model. Non-allocatable sections (debug info, notes) and unwind
// tables are not written.
//
// Far data is outside DGROUP, in private segments of class FAR_DATA
// (FAR_BSS for .farbss). A section named .fardata.<name>, .farrodata.<name>
// or .farbss.<name> of at least 4 KiB is a segment of its own, named after
// the variable (<name>_DATA, <name>_CONST, <name>_BSS). Smaller ones, and
// .fardata, .farrodata and .farbss, share the module's segments FAR_DATA,
// FAR_CONST and FAR_BSS, with a new one each time 64 KiB fills up. One over
// 64 KiB is split into consecutive segments of 64 KiB, paragraph aligned,
// which the linker places one after another, so that a huge pointer
// reaches all of it; references into it name the segment that holds the
// byte. A far section in a section group (an inline vtable) is a COMDAT
// allocated in the module's shared segment of its kind, which keeps room
// for it.
//
// Far data segments come right after the code segment, so that a startup
// module that defines empty ones places them between the code and DGROUP.
// In the memory models with far data pointers, FAR_BSS segments come after
// DGROUP instead, followed by .farend (segment FAR_END, class FAR_END),
// which marks the end of the program: at the end of the program they take
// no room in the .EXE file.
//
// Fixups are 16-bit offsets, segment-relative or self-relative, and base16
// segment fixups for references the target marks as such (`sym@SEG` on X86),
// which the DOS loader relocates. Fixups are written with no displacement
// field. As other OMF tools do, the
// displacement goes in the LEDATA bytes at the fixup location: for a target
// in this module that is its offset in its segment plus the addend.
//
// A section of its own (a function or variable with -ffunction-sections or
// -fdata-sections, or a section in a COMDAT group) whose only global symbol
// is at its start becomes an OMF COMDAT instead: a piece of _TEXT, CONST,
// _DATA or _BSS named by that symbol, which the linker may drop when nothing
// uses it. In a COMDAT group (inline functions, templates, vtables) the
// linker keeps one copy of each name ("pick any"). References to a location
// in a COMDAT go through an EXTDEF of its name.
//
// A defined global symbol named "_start" becomes the module's start address.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
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

#include <map>

using namespace llvm;

MCOMFObjectTargetWriter::~MCOMFObjectTargetWriter() = default;

namespace {

enum SegmentKind {
  SK_Text,
  SK_Const,
  SK_InitBegin,
  SK_Init,
  SK_InitEnd,
  SK_FiniBegin,
  SK_Fini,
  SK_FiniEnd,
  SK_Data,
  SK_Bss,
  SK_Stack,
  SK_Count
};

struct SegmentInfo {
  const char *Name;
  const char *Class;
  uint8_t Combine;
};

const SegmentInfo Segments[SK_Count] = {
    {"_TEXT", "CODE", OMF::SC_Public}, {"CONST", "DATA", OMF::SC_Public},
    {"XIB", "DATA", OMF::SC_Public},   {"XI", "DATA", OMF::SC_Public},
    {"XIE", "DATA", OMF::SC_Public},   {"YIB", "DATA", OMF::SC_Public},
    {"YI", "DATA", OMF::SC_Public},    {"YIE", "DATA", OMF::SC_Public},
    {"_DATA", "DATA", OMF::SC_Public}, {"_BSS", "BSS", OMF::SC_Public},
    {"STACK", "STACK", OMF::SC_Stack},
};

struct Segment {
  bool Present = false;
  unsigned Index = 0; // SEGDEF index, from 1.
  uint64_t Size = 0;
  Align MaxAlign = Align(1);
  SmallVector<const MCSection *, 4> Sections;
  // The name, class and combine type of a far data segment; the fixed
  // segments take theirs from the Segments table.
  std::string Name;
  StringRef Class;
  uint8_t Combine = OMF::SC_Public;
  // A far data segment holds bytes [PartOffset, PartOffset + Size) of its
  // one section. HasNext: the section goes on in the next segment.
  uint64_t PartOffset = 0;
  bool HasNext = false;
  // Bytes of COMDATs that the linker may add to a shared far segment.
  uint64_t Reserved = 0;
};

/// The segments of a module: the fixed kinds first, as in SegmentKind, then
/// the far data segments. A segment is named by its position here.
using SegIdx = unsigned;

struct OMFFixup {
  SegIdx Seg;       // Segment holding the fixup location.
  int Comdat = -1;  // COMDAT holding the location, or -1 for the segment.
  uint64_t Offset;  // Location offset within that segment or COMDAT.
  uint8_t LocType;  // OMF::LocationType.
  bool SegRelative; // M bit; false means self-relative.
  // Target: a segment of this module, or an external index.
  bool IsExternal;
  SegIdx TargetSeg;
  unsigned ExtIndex;
  // The target is DGROUP itself (DGROUP@SEG, as in "mov ax, DGROUP").
  bool GroupTarget = false;

  unsigned size() const { return LocType == OMF::LT_Pointer32 ? 4 : 2; }
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

  SmallVector<Segment, SK_Count + 4> Segs;
  // Placement of each written section: its segment and offset in it.
  DenseMap<const MCSection *, std::pair<SegIdx, uint64_t>> SectionMap;
  // External indices for EXTDEF and COMDEF, which share one index space.
  DenseMap<const MCSymbol *, unsigned> ExtIndices;
  SmallVector<const MCSymbol *, 16> Externals;
  std::vector<OMFFixup> Fixups;
  struct Comdat {
    const MCSectionELF *Sec;
    const MCSymbol *Sym; // Its name, at offset 0.
    SegIdx Base;
    bool PickAny; // From a COMDAT group: keep any one copy.
    int FarKind;  // For far data, its FarKind; the base is set later.
  };
  SmallVector<Comdat, 0> Comdats;
  DenseMap<const MCSection *, unsigned> ComdatIndex;
  unsigned NumLNames = 0;
  // The code segment's name: _TEXT, or <module>_TEXT with far code.
  std::string TextName;
  unsigned DGroupIndex = 0;

  bool inDGROUP(SegIdx K) const {
    return K < SK_Count &&
           (K != SK_Text || TargetObjectWriter->isCodeInDGROUP());
  }
  /// The kind of a written section; SK_Count for far data.
  std::optional<SegmentKind> classifySection(const MCSectionELF &Sec) const;
  void addFarSection(const MCSectionELF &Sec);
  /// The kinds of far data, each with its shared segment.
  enum FarKind { FK_Data, FK_Const, FK_Bss, FK_End, FK_Count };
  static FarKind getFarKind(StringRef Name) {
    return Name.starts_with(".farbss")      ? FK_Bss
           : Name.starts_with(".farrodata") ? FK_Const
           : Name.starts_with(".farend")    ? FK_End
                                            : FK_Data;
  }
  /// The module's shared far segment of a kind, with room for Size more
  /// bytes aligned to A.
  SegIdx getFarPool(FarKind Kind, uint64_t Size, Align A);
  // The current shared far segment of each kind, or 0 for none yet.
  SegIdx FarPools[FK_Count] = {};
  /// Move an offset past the end of a far data segment into the segment
  /// that holds it.
  void normalize(SegIdx &K, uint64_t &Offset) const {
    while (Segs[K].HasNext && Offset >= 0x10000) {
      ++K;
      Offset -= 0x10000;
    }
  }
  StringRef segmentName(SegIdx K) const {
    if (K == SK_Text)
      return TextName;
    return K < SK_Count ? StringRef(Segments[K].Name) : StringRef(Segs[K].Name);
  }
  StringRef segmentClass(SegIdx K) const {
    return K < SK_Count ? StringRef(Segments[K].Class) : Segs[K].Class;
  }
  uint8_t segmentCombine(SegIdx K) const {
    return K < SK_Count ? Segments[K].Combine : Segs[K].Combine;
  }
  /// The segments in SEGDEF order: the code segment, the far data segments,
  /// then the others; with far data pointers, far BSS goes last.
  SmallVector<SegIdx, 16> segmentOrder() const {
    bool BssLast = TargetObjectWriter->hasFarData();
    auto IsLate = [&](SegIdx K) {
      return BssLast && Segs[K].Class != "FAR_DATA";
    };
    SmallVector<SegIdx, 16> Order = {SK_Text};
    for (SegIdx K = SK_Count, E = Segs.size(); K != E; ++K)
      if (!IsLate(K))
        Order.push_back(K);
    for (SegIdx K = SK_Text + 1; K != SK_Count; ++K)
      Order.push_back(K);
    for (unsigned Late : {0, 1})
      for (SegIdx K = SK_Count, E = Segs.size(); K != E; ++K)
        if (IsLate(K) && (Segs[K].Class == "FAR_END") == bool(Late))
          Order.push_back(K);
    return Order;
  }
  unsigned getExternalIndex(const MCSymbol &Sym);
  bool getSymbolLocation(const MCSymbol &Sym, SegIdx &Seg,
                         uint64_t &Offset) const;
  bool getComdatLocation(const MCSymbol &Sym, unsigned &Idx,
                         uint64_t &Offset) const;
  void findComdats();
  void writeComdats();
  void writeExports();

  void writeHeader();
  void writeNamesAndSegments();
  void writeExternals();
  void writePublics();
  void writeSegmentData(SegIdx K);
  void writeFixups(SegIdx K, uint64_t Start, uint64_t End,
                   ArrayRef<const OMFFixup *> SegFixups);
  void writeModuleEnd();

public:
  OMFObjectWriter(std::unique_ptr<MCOMFObjectTargetWriter> MOTW,
                  raw_pwrite_stream &OS)
      : TargetObjectWriter(std::move(MOTW)), OS(OS) {}

  void reset() override {
    Segs.clear();
    SectionMap.clear();
    ExtIndices.clear();
    Externals.clear();
    Fixups.clear();
    Comdats.clear();
    ComdatIndex.clear();
    std::fill(std::begin(FarPools), std::end(FarPools), 0);
    NumLNames = 0;
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
  for (StringRef Far : {".fardata", ".farrodata", ".farbss", ".farend"})
    if (Name == Far || Name.starts_with((Far + ".").str()))
      return SK_Count;
  for (SegmentKind K : {SK_InitBegin, SK_InitEnd, SK_FiniBegin, SK_FiniEnd})
    if (Name == Segments[K].Name)
      return K;
  // As in the usual ELF linker scripts, .init_array* includes names such as
  // picolibc's .fini_array_onexit. Init priorities in the names are not
  // kept: the linker puts the entries in module order.
  auto IsTable = [&](StringRef Prefix) {
    return Name == Prefix || Name.starts_with((Prefix + ".").str());
  };
  if (Name.starts_with(".init_array") || Name.starts_with(".preinit_array") ||
      IsTable(".ctors"))
    return SK_Init;
  if (Name.starts_with(".fini_array") || IsTable(".dtors"))
    return SK_Fini;
  if (Flags & ELF::SHF_EXECINSTR)
    return SK_Text;
  if (Sec.getType() == ELF::SHT_NOBITS)
    return SK_Bss;
  if (Flags & ELF::SHF_WRITE)
    return SK_Data;
  return SK_Const;
}

void OMFObjectWriter::findComdats() {
  // The global symbols defined in each section. A section that an alias
  // points into is not a COMDAT, since OMF cannot export the alias.
  DenseMap<const MCSection *, SmallVector<const MCSymbol *, 1>> Globals;
  DenseSet<const MCSection *> Excluded;
  std::map<std::pair<const MCSymbol *, StringRef>, unsigned> GroupParts;
  for (const MCSymbol &S : Asm->symbols()) {
    const auto &Sym = static_cast<const MCSymbolELF &>(S);
    if (Sym.isVariable()) {
      const MCSymbol *Base = Asm->getBaseSymbol(Sym);
      if (Base && Base->isInSection())
        Excluded.insert(&Base->getSection());
      continue;
    }
    if (!Sym.isInSection() || Sym.isTemporary() || !Sym.isBindingSet() ||
        Sym.getBinding() == ELF::STB_LOCAL)
      continue;
    Globals[&Sym.getSection()].push_back(&Sym);
  }

  for (const MCSection &S : *Asm) {
    const auto &Sec = static_cast<const MCSectionELF &>(S);
    std::optional<SegmentKind> K = classifySection(Sec);
    if (!K || (*K != SK_Text && *K != SK_Const && *K != SK_Data &&
               *K != SK_Bss && *K != SK_Count))
      continue;
    // Only a section of its own: in a group, or named like .text.foo. Far
    // data only in a group: other far variables can be over 64 KiB.
    if (!Sec.getGroup() &&
        (*K == SK_Count || Sec.getName().find('.', 1) == StringRef::npos))
      continue;
    if (Excluded.count(&Sec) || Asm->getSectionAddressSize(Sec) == 0)
      continue;
    auto It = Globals.find(&Sec);
    const MCSymbol *Sym = nullptr;
    if (It == Globals.end()) {
      // A group section with no global symbol, such as a function's jump
      // table, is named after its group and section, so that it is kept
      // with the function and its copies in other modules merge.
      const MCSymbolELF *Sig = Sec.getGroup();
      if (!Sig)
        continue;
      // The section name repeats the function name, so only its first
      // part (.rodata) is kept, with a count for a second one.
      StringRef Kind = Sec.getName().substr(1).split('.').first;
      unsigned &N = GroupParts[{Sig, Kind}];
      Sym = getContext().getOrCreateSymbol(Sig->getName() + "$" + Kind +
                                           (N++ ? Twine(N - 1) : Twine()));
    } else {
      if (It->second.size() != 1)
        continue;
      Sym = It->second.front();
      uint64_t Offset;
      if (!Asm->getSymbolOffset(*Sym, Offset) || Offset != 0)
        continue;
    }
    if (Sym->getName().size() > 255)
      continue;
    ComdatIndex[&Sec] = Comdats.size();
    Comdats.push_back({&Sec, Sym, *K, Sec.getGroup() != nullptr,
                       *K == SK_Count ? getFarKind(Sec.getName()) : -1});
  }
}

static StringRef farSuffix(unsigned Kind) {
  static const char *const Suffixes[] = {"_DATA", "_CONST", "_BSS", "_END"};
  return Suffixes[Kind];
}

SegIdx OMFObjectWriter::getFarPool(FarKind Kind, uint64_t Size, Align A) {
  SegIdx &K = FarPools[Kind];
  if (K && alignTo(Segs[K].Size + Segs[K].Reserved, A) + Size <= 0x10000)
    return K;
  K = Segs.size();
  Segment &Seg = Segs.emplace_back();
  Seg.Present = true;
  Seg.Name = ("FAR" + farSuffix(Kind)).str();
  Seg.Class = Kind == FK_Bss   ? "FAR_BSS"
              : Kind == FK_End ? "FAR_END"
                               : "FAR_DATA";
  Seg.Combine = OMF::SC_Private;
  return K;
}

void OMFObjectWriter::addFarSection(const MCSectionELF &Sec) {
  StringRef Name = Sec.getName();
  FarKind Kind = getFarKind(Name);
  StringRef Var = Name.split('.').second.split('.').second;
  uint64_t Size = Asm->getSectionAddressSize(Sec);
  // Small far variables share a segment.
  constexpr uint64_t MinOwnSegment = 0x1000;
  if (Var.empty() || Size < MinOwnSegment || Kind == FK_End) {
    SegIdx K = getFarPool(Kind, Size, Sec.getAlign());
    Segment &Seg = Segs[K];
    Seg.Size = alignTo(Seg.Size, Sec.getAlign());
    Seg.MaxAlign = std::max(Seg.MaxAlign, Sec.getAlign());
    SectionMap[&Sec] = {K, Seg.Size};
    Seg.Sections.push_back(&Sec);
    Seg.Size += Size;
    return;
  }
  bool Bss = Kind == FK_Bss;
  std::string SegName = (Var.take_front(240) + farSuffix(Kind)).str();
  bool Split = Size > 0x10000;
  SectionMap[&Sec] = {Segs.size(), 0};
  uint64_t Offset = 0;
  do {
    Segment &Seg = Segs.emplace_back();
    Seg.Present = true;
    Seg.Name = SegName;
    Seg.Class = Bss ? "FAR_BSS" : "FAR_DATA";
    Seg.Combine = OMF::SC_Private;
    Seg.Sections.push_back(&Sec);
    Seg.PartOffset = Offset;
    Seg.Size = std::min<uint64_t>(Size - Offset, 0x10000);
    // The parts of a split section are paragraph aligned, so that each
    // starts where the one before it ends.
    Seg.MaxAlign = Split ? std::max(Sec.getAlign(), Align(16)) : Sec.getAlign();
    Offset += 0x10000;
    Seg.HasNext = Offset < Size;
  } while (Offset < Size);
}

void OMFObjectWriter::executePostLayoutBinding() {
  findComdats();

  // Place sections in segments.
  Segs.resize(SK_Count);
  std::fill(std::begin(FarPools), std::end(FarPools), 0);
  for (const MCSection &S : *Asm) {
    const auto &Sec = static_cast<const MCSectionELF &>(S);
    if (ComdatIndex.count(&Sec))
      continue;
    std::optional<SegmentKind> K = classifySection(Sec);
    if (!K)
      continue;
    if (*K == SK_Count) {
      addFarSection(Sec);
      continue;
    }
    Segment &Seg = Segs[*K];
    Seg.Present = true;
    Seg.Size = alignTo(Seg.Size, Sec.getAlign());
    Seg.MaxAlign = std::max(Seg.MaxAlign, Sec.getAlign());
    SectionMap[&Sec] = {*K, Seg.Size};
    Seg.Sections.push_back(&Sec);
    Seg.Size += Asm->getSectionAddressSize(Sec);
  }
  // Far COMDATs are allocated in the shared far segment of their kind.
  // Room is kept for them, so that the segment stays under 64 KiB whichever
  // copies the linker keeps.
  for (Comdat &C : Comdats)
    if (C.FarKind >= 0) {
      uint64_t Size = Asm->getSectionAddressSize(*C.Sec);
      C.Base = getFarPool(FarKind(C.FarKind), Size, C.Sec->getAlign());
      Segs[C.Base].Reserved =
          alignTo(Segs[C.Base].Reserved, C.Sec->getAlign()) + Size;
    }

  // Linkers order segments by class in the order they first see them, so
  // every module defines the usual segments, even empty, in the usual order
  // (as DOS compilers do). Then code, data and BSS stay in this order
  // whichever module comes first. STACK is only defined where it is used.
  for (SegmentKind K : {SK_Text, SK_Const, SK_Data, SK_Bss})
    Segs[K].Present = true;
  // The constructor and destructor segments go together.
  bool AnyTable = false;
  for (unsigned K = SK_InitBegin; K <= SK_FiniEnd; ++K)
    AnyTable |= Segs[K].Present;
  // They are all word aligned, so that the linker leaves no padding
  // between the begin label, the table and the end label.
  if (AnyTable)
    for (unsigned K = SK_InitBegin; K <= SK_FiniEnd; ++K) {
      Segs[K].Present = true;
      Segs[K].MaxAlign = std::max(Segs[K].MaxAlign, Align(2));
    }

  // SEGDEF order: the code segment, the far data segments, then DGROUP.
  unsigned Index = 0;
  bool AnyInGroup = false;
  for (SegIdx K : segmentOrder()) {
    if (!Segs[K].Present)
      continue;
    Segs[K].Index = ++Index;
    AnyInGroup |= inDGROUP(K);
    if (Segs[K].Size > 0x10000)
      getContext().reportError(SMLoc(), Twine("segment ") + segmentName(K) +
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

bool OMFObjectWriter::getComdatLocation(const MCSymbol &Sym, unsigned &Idx,
                                        uint64_t &Offset) const {
  const MCSymbol *Base = Asm->getBaseSymbol(Sym);
  if (!Base || !Base->isInSection())
    return false;
  auto It = ComdatIndex.find(&Base->getSection());
  if (It == ComdatIndex.end() || !Asm->getSymbolOffset(Sym, Offset))
    return false;
  Idx = It->second;
  return true;
}

bool OMFObjectWriter::getSymbolLocation(const MCSymbol &Sym, SegIdx &Seg,
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
  normalize(Seg, Offset);
  return true;
}

void OMFObjectWriter::recordRelocation(const MCFragment &F,
                                       const MCFixup &Fixup, MCValue Target,
                                       uint64_t &FixedValue) {
  auto SecIt = SectionMap.find(F.getParent());
  auto CdIt = ComdatIndex.find(F.getParent());
  // Fixups in sections that are not written (debug info) are dropped.
  if (SecIt == SectionMap.end() && CdIt == ComdatIndex.end()) {
    FixedValue = 0;
    return;
  }
  MCContext &Ctx = getContext();
  SMLoc Loc = Fixup.getLoc();

  bool SegRef =
      Target.getSpecifier() && TargetObjectWriter->isSegmentReference(Target);
  if (Target.getSubSym() || (Target.getSpecifier() && !SegRef)) {
    Ctx.reportError(Loc, "unsupported relocation expression in OMF object");
    return;
  }
  const MCSymbol *Sym = Target.getAddSym();
  if (!Sym) {
    Ctx.reportError(Loc, "unsupported absolute PC-relative fixup in OMF");
    return;
  }

  bool PCRel = Fixup.isPCRel();
  if (SegRef && PCRel) {
    Ctx.reportError(Loc, "a segment reference cannot be PC-relative");
    return;
  }
  // A 4-byte reference is a far pointer, segment:offset.
  bool FarPtr = false;
  switch (Fixup.getKind()) {
  case FK_Data_2:
    break;
  case FK_Data_4:
    if (PCRel || SegRef) {
      Ctx.reportError(Loc, "a far pointer cannot be PC-relative or a segment "
                           "reference");
      return;
    }
    FarPtr = true;
    break;
  default:
    Ctx.reportError(Loc, "unsupported relocation size in OMF object; only "
                         "16-bit offsets and far pointers are supported");
    return;
  }

  OMFFixup R;
  if (CdIt != ComdatIndex.end()) {
    R.Comdat = CdIt->second;
    R.Seg = Comdats[CdIt->second].Base;
    R.Offset = Asm->getFragmentOffset(F) + Fixup.getOffset();
  } else {
    R.Seg = SecIt->second.first;
    R.Offset =
        SecIt->second.second + Asm->getFragmentOffset(F) + Fixup.getOffset();
    normalize(R.Seg, R.Offset);
  }
  R.LocType = FarPtr   ? OMF::LT_Pointer32
              : SegRef ? OMF::LT_Base16
                       : OMF::LT_Offset16;
  R.SegRelative = !PCRel;

  int64_t Addend = Target.getConstant();
  // The X86 backend has already biased PC-relative constants by the field
  // size; the self-relative OMF fixup does that itself.
  if (PCRel)
    Addend += 2;

  SegIdx TargetSeg;
  uint64_t TargetOffset;
  unsigned TargetComdat;
  if (SegRef && Sym->isUndefined() && Sym->getName() == "DGROUP") {
    if (!DGroupIndex) {
      Ctx.reportError(Loc, "DGROUP@SEG in a module without DGROUP");
      return;
    }
    R.IsExternal = false;
    R.GroupTarget = true;
    R.TargetSeg = SK_Text;
    R.ExtIndex = 0;
  } else if (!Sym->isCommon() && getComdatLocation(*Sym, TargetComdat, TargetOffset)) {
    // A COMDAT is reached through an external of its name.
    R.IsExternal = true;
    R.TargetSeg = SK_Text;
    R.ExtIndex = getExternalIndex(*Comdats[TargetComdat].Sym);
    Addend += TargetOffset;
  } else if (!Sym->isCommon() &&
             getSymbolLocation(*Sym, TargetSeg, TargetOffset)) {
    if (SegRef) {
      // The addend of a segment reference is in paragraphs. The parts of a
      // split far section are 64 KiB apart, so each 0x1000 of it moves to
      // the next part.
      while (Addend >= 0x1000 && TargetSeg + 1 < Segs.size() &&
             Segs[TargetSeg].HasNext) {
        ++TargetSeg;
        Addend -= 0x1000;
      }
    } else {
      // The addend can reach into a later part of a split far section.
      uint64_t Total = TargetOffset + Addend;
      if (Addend > 0)
        normalize(TargetSeg, Total);
      Addend = Total;
    }
    R.IsExternal = false;
    R.TargetSeg = TargetSeg;
    R.ExtIndex = 0;
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

  // The linker and the loader add the frame number to a base16 field, so it
  // holds the addend in paragraphs.
  FixedValue = uint64_t(Addend) & 0xffff;
  Fixups.push_back(R);
}

void OMFObjectWriter::writeHeader() {
  std::string Name = "noname";
  if (!getFileNames().empty())
    Name = sys::path::filename(getFileNames().front().first).str();
  if (Name.size() > 255)
    Name.resize(255);

  // With far calls, each module has a code segment of its own, named after
  // the source file as other DOS compilers do (hello.c: hello_TEXT).
  TextName = Segments[SK_Text].Name;
  if (TargetObjectWriter->hasFarCode()) {
    std::string Stem = sys::path::stem(Name).str();
    for (char &C : Stem)
      if (!isAlnum(C) && C != '_' && C != '$')
        C = '_';
    TextName = Stem.substr(0, 200) + "_TEXT";
  }
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
  SmallVector<unsigned, 16> SegName(Segs.size()), ClassName(Segs.size());
  for (SegIdx K : segmentOrder()) {
    if (!Segs[K].Present)
      continue;
    SegName[K] = NameIndex(segmentName(K));
    ClassName[K] = NameIndex(segmentClass(K));
  }
  Record LNames(OMF::LNAMES);
  for (StringRef N : Names)
    LNames.name(N);
  LNames.write(OS);
  NumLNames = Names.size();

  for (SegIdx K : segmentOrder()) {
    const Segment &Seg = Segs[K];
    if (!Seg.Present)
      continue;
    uint8_t A = Seg.MaxAlign.value() == 1    ? OMF::SA_Byte
                : Seg.MaxAlign.value() == 2  ? OMF::SA_Word
                : Seg.MaxAlign.value() <= 16 ? OMF::SA_Para
                                             : OMF::SA_Page;
    bool Big = Seg.Size == 0x10000;
    Record SegDef(OMF::SEGDEF);
    SegDef.byte((A << 5) | (segmentCombine(K) << 2) | (Big << 1));
    SegDef.word(Big ? 0 : Seg.Size);
    SegDef.index(SegName[K]);
    SegDef.index(ClassName[K]);
    SegDef.index(1); // Overlay name: the empty name.
    SegDef.write(OS);
  }

  if (DGroupIndex) {
    Record GrpDef(OMF::GRPDEF);
    GrpDef.index(DGroupName);
    for (SegIdx K : segmentOrder()) {
      if (!Segs[K].Present || !inDGROUP(K))
        continue;
      GrpDef.byte(0xff);
      GrpDef.index(Segs[K].Index);
    }
    GrpDef.write(OS);
  }
}

void OMFObjectWriter::writeExternals() {
  // OMF has no undefined weak symbols. A WKEXT comment makes a weak
  // reference resolve to a default external when nothing defines it. The
  // default here is __omf_weak_null. The startup code, which is linked
  // first, defines it at offset 0 of _TEXT, so an undefined weak symbol has
  // offset 0 as in ELF and can still be the target of a near call.
  SmallVector<std::pair<unsigned, unsigned>, 4> Weak;
  for (unsigned I = 0, E = Externals.size(); I != E; ++I) {
    const auto *Sym = static_cast<const MCSymbolELF *>(Externals[I]);
    if (Sym->isCommon() || !Sym->isUndefined() ||
        Sym->getBinding() != ELF::STB_WEAK)
      continue;
    MCSymbol *Null = getContext().getOrCreateSymbol("__omf_weak_null");
    if (Null == Sym || !Null->isUndefined())
      continue;
    Weak.push_back({I + 1, getExternalIndex(*Null)});
  }

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

  if (Weak.empty())
    return;
  Record WkExt(OMF::COMENT);
  auto StartWkExt = [&] {
    WkExt.byte(0x80); // Attributes: no purge.
    WkExt.byte(OMF::CC_WeakExtern);
  };
  StartWkExt();
  for (auto [WeakIdx, DefaultIdx] : Weak) {
    if (WkExt.size() + 4 > OMF::MaxRecordData) {
      WkExt.write(OS);
      WkExt.clear();
      StartWkExt();
    }
    WkExt.index(WeakIdx);
    WkExt.index(DefaultIdx);
  }
  WkExt.write(OS);
}

void OMFObjectWriter::writePublics() {
  // Group publics by segment. Absolute symbols use segment 0.
  // The last entry holds the absolute symbols.
  const SegIdx Absolute = Segs.size();
  SmallVector<SmallVector<std::pair<StringRef, uint64_t>, 16>, 16> Publics(
      Segs.size() + 1);
  for (const MCSymbol &S : Asm->symbols()) {
    const auto &Sym = static_cast<const MCSymbolELF &>(S);
    if (Sym.isTemporary() || Sym.isCommon() || !Sym.isBindingSet() ||
        Sym.getBinding() == ELF::STB_LOCAL)
      continue;
    if (Sym.isInSection() && Sym.getSection().getBeginSymbol() == &Sym)
      continue;
    // A COMDAT names itself.
    unsigned CdIdx;
    uint64_t CdOffset;
    if (getComdatLocation(Sym, CdIdx, CdOffset))
      continue;
    SegIdx K;
    uint64_t Offset;
    if (getSymbolLocation(Sym, K, Offset)) {
      Publics[K].push_back({Sym.getName(), Offset});
      continue;
    }
    if (Sym.isVariable()) {
      int64_t Value;
      if (Sym.getVariableValue()->evaluateAsAbsolute(Value, *Asm)) {
        Publics[Absolute].push_back({Sym.getName(), uint64_t(Value)});
        continue;
      }
    }
    if (!Sym.isUndefined())
      getContext().reportError(SMLoc(), "symbol '" + Sym.getName() +
                                            "' cannot be exported to OMF");
  }

  for (SegIdx K = 0; K <= Absolute; ++K) {
    if (Publics[K].empty())
      continue;
    Record PubDef(OMF::PUBDEF);
    auto Start = [&] {
      if (K == Absolute) {
        PubDef.index(0);
        PubDef.index(0);
        PubDef.word(0); // Frame number.
      } else {
        PubDef.index(inDGROUP(K) ? DGroupIndex : 0);
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

void OMFObjectWriter::writeFixups(SegIdx K, uint64_t Start, uint64_t End,
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
    bool GroupFrame = R->GroupTarget || (R->IsExternal
                                             ? TargetObjectWriter->isCodeInDGROUP()
                                             : inDGROUP(R->TargetSeg));
    uint8_t Frame = GroupFrame ? OMF::F_Group : OMF::F_Target;
    uint8_t Method = R->GroupTarget  ? OMF::T_GroupNoDisp
                     : R->IsExternal ? OMF::T_ExternalNoDisp
                                     : OMF::T_SegmentNoDisp;
    FixUpp.byte((Frame << 4) | Method);
    if (GroupFrame)
      FixUpp.index(DGroupIndex);
    FixUpp.index(R->GroupTarget ? DGroupIndex
                 : R->IsExternal ? R->ExtIndex
                                 : Segs[R->TargetSeg].Index);
  }
  if (!FixUpp.empty())
    FixUpp.write(OS);
}

void OMFObjectWriter::writeSegmentData(SegIdx K) {
  const Segment &Seg = Segs[K];
  if (!Seg.Present || Seg.Size == 0)
    return;

  // Build the segment image. Bytes of virtual (bss) sections stay zero.
  SmallVector<uint8_t, 0> Image(Seg.Size, 0);
  bool AnyData = false;
  uint64_t PrevEnd = 0;
  // A part of a far section split into 64 KiB segments.
  bool Part = K >= SK_Count && (Seg.PartOffset || Seg.HasNext);
  if (Part) {
    const MCSection *Sec = Seg.Sections.front();
    if (!Sec->isBssSection()) {
      SmallString<256> Buf;
      raw_svector_ostream VOS(Buf);
      Asm->writeSectionData(VOS, Sec);
      std::copy(Buf.begin() + Seg.PartOffset,
                Buf.begin() + Seg.PartOffset + Seg.Size, Image.begin());
      AnyData = true;
    }
  }
  for (const MCSection *Sec : Seg.Sections) {
    if (Part)
      break;
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
    if (R.Seg != K || R.Comdat >= 0)
      continue;
    SegFixups.push_back(&R);
    for (unsigned I = 0; I != R.size(); ++I)
      InFixup[R.Offset + I] = true;
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
      if (R->Offset < End && R->Offset + R->size() > End)
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

void OMFObjectWriter::writeComdats() {
  if (Comdats.empty())
    return;

  // COMDAT names are LNAMES, numbered after the segment and class names.
  Record LNames(OMF::LNAMES);
  for (const Comdat &C : Comdats) {
    StringRef Name = C.Sym->getName();
    if (LNames.size() + Name.size() + 1 > OMF::MaxRecordData) {
      LNames.write(OS);
      LNames.clear();
    }
    LNames.name(Name);
  }
  LNames.write(OS);

  for (unsigned CI = 0, CE = Comdats.size(); CI != CE; ++CI) {
    const Comdat &C = Comdats[CI];
    uint64_t Size = Asm->getSectionAddressSize(*C.Sec);
    if (Size > 0xffff) {
      getContext().reportError(SMLoc(), "section " + C.Sec->getName() +
                                            " is larger than 64 KiB");
      continue;
    }
    SmallVector<uint8_t, 0> Image(Size, 0);
    if (!C.Sec->isBssSection()) {
      SmallString<256> Buf;
      raw_svector_ostream VOS(Buf);
      Asm->writeSectionData(VOS, C.Sec);
      std::copy(Buf.begin(), Buf.end(), Image.begin());
    }
    SmallVector<const OMFFixup *, 8> CdFixups;
    for (const OMFFixup &R : Fixups)
      if (R.Comdat == int(CI))
        CdFixups.push_back(&R);

    Align A = C.Sec->getAlign();
    uint8_t AlignCode = A.value() == 1   ? OMF::SA_Byte
                        : A.value() == 2 ? OMF::SA_Word
                                         : OMF::SA_Para;
    // Header: flags, attributes, align, offset (2), type, group, segment,
    // name; at most 10 bytes.
    const uint64_t MaxChunk = OMF::MaxRecordData - 12;
    for (uint64_t I = 0; I < Size;) {
      uint64_t End = std::min(Size, I + MaxChunk);
      // Do not split a fixup field across two records.
      for (const OMFFixup *R : CdFixups)
        if (R->Offset < End && R->Offset + R->size() > End)
          End = R->Offset;
      Record Cd(OMF::COMDAT);
      Cd.byte(I == 0 ? 0x00 : 0x01); // Continuation flag.
      // Selection in the high nibble; allocation 0 is the explicit base.
      Cd.byte(C.PickAny ? 0x10 : 0x00);
      Cd.byte(AlignCode);
      Cd.word(I);
      Cd.index(0); // Type index.
      Cd.index(inDGROUP(C.Base) ? DGroupIndex : 0);
      Cd.index(Segs[C.Base].Index);
      Cd.index(NumLNames + CI + 1);
      Cd.bytes(ArrayRef(Image).slice(I, End - I));
      Cd.write(OS);
      writeFixups(C.Base, I, End, CdFixups);
      I = End;
    }
  }
}

/// An EXPDEF comment for each name in .omf.exports (dllexport symbols, with
/// NUL after each), so that a linker making a Windows or OS/2 program or DLL
/// exports them under the same name.
void OMFObjectWriter::writeExports() {
  for (const MCSection &S : *Asm) {
    if (S.getName() != ".omf.exports")
      continue;
    SmallString<256> Buf;
    raw_svector_ostream VOS(Buf);
    Asm->writeSectionData(VOS, &S);
    StringRef Names = Buf;
    while (!Names.empty()) {
      auto [Name, Rest] = Names.split('\0');
      Names = Rest;
      if (Name.empty() || Name.size() > 255)
        continue;
      Record Coment(OMF::COMENT);
      Coment.byte(0x00); // Attributes.
      Coment.byte(OMF::CC_OMFExtension);
      Coment.byte(0x02); // EXPDEF.
      Coment.byte(0x00); // Flags: by name, no ordinal.
      Coment.name(Name); // Exported name.
      Coment.name("");   // Internal name: the same.
      Coment.write(OS);
    }
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
  SegIdx K;
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
  writeExports();
  for (SegIdx K : segmentOrder())
    writeSegmentData(K);
  writeComdats();
  writeModuleEnd();
  return OS.tell() - StartOffset;
}

std::unique_ptr<MCObjectWriter>
llvm::createOMFObjectWriter(std::unique_ptr<MCOMFObjectTargetWriter> MOTW,
                            raw_pwrite_stream &OS) {
  return std::make_unique<OMFObjectWriter>(std::move(MOTW), OS);
}
