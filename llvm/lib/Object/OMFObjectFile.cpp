//===- OMFObjectFile.cpp - OMF object file implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Reads OMF objects (TIS OMF 1.1), 16-bit and 32-bit record forms. The whole
// module is parsed when the file is opened.
//
// Supported records: THEADR, LHEADR, LNAMES, LLNAMES, SEGDEF, GRPDEF,
// EXTDEF, LEXTDEF, CEXTDEF, COMDEF, LCOMDEF, PUBDEF, LPUBDEF, ALIAS, LEDATA,
// LIDATA, FIXUPP (with threads), COMDAT and MODEND. COMDATs define symbols
// but their data and fixups are not kept. Others (COMENT, LINNUM, ...) are
// skipped.
//
//===----------------------------------------------------------------------===//

#include "llvm/Object/OMFObjectFile.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/BinaryFormat/OMF.h"
#include "llvm/Object/Error.h"

using namespace llvm;
using namespace llvm::object;

namespace {

/// Reads the fields of one record, reporting reads past its end.
class RecordReader {
  ArrayRef<uint8_t> Data;
  size_t Pos = 0;
  bool Failed = false;

public:
  explicit RecordReader(ArrayRef<uint8_t> Data) : Data(Data) {}

  bool more() const { return !Failed && Pos < Data.size(); }
  bool failed() const { return Failed; }
  size_t pos() const { return Pos; }

  uint8_t u8() {
    if (Pos >= Data.size()) {
      Failed = true;
      return 0;
    }
    return Data[Pos++];
  }
  uint16_t u16() {
    uint16_t Lo = u8();
    return Lo | uint16_t(u8()) << 8;
  }
  uint32_t u32() {
    uint32_t Lo = u16();
    return Lo | uint32_t(u16()) << 16;
  }
  /// A 2-byte or 4-byte field, as the record's 16/32-bit form says.
  uint32_t word(bool Is32) { return Is32 ? u32() : u16(); }
  unsigned index() {
    unsigned B = u8();
    if (B & 0x80)
      B = (B & 0x7f) << 8 | u8();
    return B;
  }
  StringRef name() {
    unsigned Len = u8();
    if (Pos + Len > Data.size()) {
      Failed = true;
      return StringRef();
    }
    StringRef S(reinterpret_cast<const char *>(Data.data() + Pos), Len);
    Pos += Len;
    return S;
  }
  ArrayRef<uint8_t> rest() {
    ArrayRef<uint8_t> R = Data.drop_front(Pos);
    Pos = Data.size();
    return R;
  }
  /// The variable-length size field of a COMDEF entry.
  uint32_t communalLength() {
    uint8_t B = u8();
    if (B < 0x80)
      return B;
    switch (B) {
    case 0x81:
      return u16();
    case 0x84: {
      uint32_t Lo = u16();
      return Lo | uint32_t(u8()) << 16;
    }
    case 0x88:
      return u32();
    }
    Failed = true;
    return 0;
  }
};

struct Thread {
  bool Valid = false;
  uint8_t Method = 0;
  unsigned Index = 0;
};

} // end anonymous namespace

static Error malformed(const Twine &Msg) {
  return make_error<GenericBinaryError>("malformed OMF object: " + Msg,
                                        object_error::parse_failed);
}

Expected<std::unique_ptr<OMFObjectFile>>
OMFObjectFile::create(MemoryBufferRef Object) {
  std::unique_ptr<OMFObjectFile> Obj(new OMFObjectFile(Object));
  if (Error E = Obj->parse())
    return std::move(E);
  return std::move(Obj);
}

Expected<std::unique_ptr<ObjectFile>>
ObjectFile::createOMFObjectFile(MemoryBufferRef Object) {
  Expected<std::unique_ptr<OMFObjectFile>> Obj = OMFObjectFile::create(Object);
  if (!Obj)
    return Obj.takeError();
  return std::move(*Obj);
}

/// Expand an LIDATA data block into Out.
static bool expandIteratedBlock(RecordReader &R, bool Is32,
                                std::vector<uint8_t> &Out, unsigned Depth) {
  if (Depth > 16)
    return false;
  uint32_t Repeat = R.word(Is32);
  uint16_t Blocks = R.u16();
  std::vector<uint8_t> Body;
  if (Blocks == 0) {
    uint8_t Count = R.u8();
    for (unsigned I = 0; I != Count; ++I)
      Body.push_back(R.u8());
  } else {
    for (unsigned I = 0; I != Blocks; ++I)
      if (!expandIteratedBlock(R, Is32, Body, Depth + 1))
        return false;
  }
  if (R.failed() || uint64_t(Repeat) * Body.size() > 0x1000000)
    return false;
  for (uint32_t I = 0; I != Repeat; ++I)
    Out.insert(Out.end(), Body.begin(), Body.end());
  return true;
}

Error OMFObjectFile::parse() {
  ArrayRef<uint8_t> Buf(
      reinterpret_cast<const uint8_t *>(getData().data()), getData().size());
  SmallVector<StringRef, 32> Names = {StringRef()}; // Indices start at 1.
  SmallVector<StringRef, 4> Groups = {StringRef()};
  SmallVector<int32_t, 32> Externals = {-1}; // External index to symbol.
  SmallVector<std::pair<StringRef, StringRef>, 4> Aliases;
  int32_t LastDataSeg = -1;
  uint32_t LastDataOffset = 0;
  // Fixups after a COMDAT record apply to its data, which this reader does
  // not place in a segment, so they are skipped.
  bool LastDataIsComdat = false;
  Thread FrameThreads[4], TargetThreads[4];

  size_t Pos = 0;
  bool SeenEnd = false;
  while (Pos < Buf.size() && !SeenEnd) {
    if (Pos + 3 > Buf.size())
      return malformed("truncated record header at offset " + Twine(Pos));
    uint8_t Type = Buf[Pos];
    uint16_t Len = Buf[Pos + 1] | uint16_t(Buf[Pos + 2]) << 8;
    if (Len == 0 || Pos + 3 + Len > Buf.size())
      return malformed("record at offset " + Twine(Pos) +
                       " extends past the end of the file");
    ArrayRef<uint8_t> Whole = Buf.slice(Pos, 3 + Len);
    // A zero checksum byte means the checksum was not computed.
    if (Whole.back() != 0) {
      uint8_t Sum = 0;
      for (uint8_t B : Whole)
        Sum += B;
      if (Sum != 0)
        return malformed("bad checksum in record at offset " + Twine(Pos));
    }
    size_t RecordPos = Pos;
    Pos += 3 + Len;

    RecordReader R(Whole.slice(3, Len - 1));
    bool Is32 = Type & 1;
    auto Bad = [&](const Twine &What) {
      return malformed(What + " in record at offset " + Twine(RecordPos));
    };
    auto CheckName = [&](unsigned I) { return I < Names.size(); };
    auto CheckSeg = [&](unsigned I) { return I >= 1 && I <= Segments.size(); };

    switch (Type & ~1) {
    case 0x80: // THEADR
    case 0x82: // LHEADR
      ModuleName = R.name();
      break;

    case OMF::LNAMES:
    case 0xCA: // LLNAMES
      while (R.more())
        Names.push_back(R.name());
      break;

    case OMF::SEGDEF: {
      Segment S;
      uint8_t ACBP = R.u8();
      uint8_t A = ACBP >> 5;
      if (A == OMF::SA_Absolute) {
        R.u16(); // Frame number.
        R.u8();  // Offset.
      }
      static const uint16_t Aligns[] = {1, 1, 2, 16, 256, 4, 1, 1};
      S.Align = std::min<uint16_t>(Aligns[A], 255);
      if (A == OMF::SA_Page)
        S.Align = 0; // 256; see getSectionAlignment.
      S.Combine = (ACBP >> 2) & 7;
      S.Use32 = ACBP & 1;
      uint32_t Size = R.word(Is32);
      if (ACBP & 2) {
        if (Is32)
          return Bad("a 4 GiB segment");
        Size = 0x10000;
      }
      S.Size = Size;
      unsigned NameIdx = R.index(), ClassIdx = R.index();
      R.index(); // Overlay name.
      if (R.failed() || !CheckName(NameIdx) || !CheckName(ClassIdx))
        return Bad("bad SEGDEF");
      S.Name = Names[NameIdx];
      S.Class = Names[ClassIdx];
      Segments.push_back(std::move(S));
      break;
    }

    case OMF::GRPDEF: {
      unsigned NameIdx = R.index();
      if (!CheckName(NameIdx))
        return Bad("bad GRPDEF name");
      Groups.push_back(Names[NameIdx]);
      while (R.more()) {
        uint8_t Kind = R.u8();
        unsigned Seg = R.index();
        if (Kind != 0xff || !CheckSeg(Seg))
          return Bad("bad GRPDEF entry");
        Segments[Seg - 1].Group = Groups.back();
      }
      break;
    }

    case OMF::EXTDEF:
    case 0xB4: // LEXTDEF
      while (R.more()) {
        StringRef Name = R.name();
        R.index(); // Type index.
        Externals.push_back(Symbols.size());
        Symbols.push_back({Name, SK_Undefined, (Type & ~1) == 0xB4, -1, 0, 0});
      }
      break;

    case 0xBC: // CEXTDEF
      while (R.more()) {
        unsigned NameIdx = R.index();
        R.index(); // Type index.
        if (!CheckName(NameIdx))
          return Bad("bad CEXTDEF name");
        Externals.push_back(Symbols.size());
        Symbols.push_back({Names[NameIdx], SK_Undefined, false, -1, 0, 0});
      }
      break;

    case OMF::COMDEF:
    case 0xB8: // LCOMDEF
      while (R.more()) {
        StringRef Name = R.name();
        R.index(); // Type index.
        uint8_t DataType = R.u8();
        uint64_t Size = R.communalLength();
        if (DataType == OMF::CT_Far)
          Size *= R.communalLength();
        if (R.failed() || Size > UINT32_MAX)
          return Bad("bad COMDEF");
        Externals.push_back(Symbols.size());
        Symbols.push_back({Name, SK_Common, (Type & ~1) == 0xB8, -1, 0,
                           uint32_t(Size)});
      }
      break;

    case OMF::PUBDEF:
    case 0xB6: { // LPUBDEF
      R.index(); // Group.
      unsigned Seg = R.index();
      if (Seg == 0)
        R.u16(); // Frame number.
      else if (!CheckSeg(Seg))
        return Bad("bad PUBDEF segment");
      while (R.more()) {
        StringRef Name = R.name();
        uint32_t Offset = R.word(Is32);
        R.index(); // Type index.
        Symbols.push_back({Name, Seg ? SK_Defined : SK_Absolute,
                           (Type & ~1) == 0xB6, int32_t(Seg) - 1, Offset, 0});
      }
      break;
    }

    case OMF::LEDATA:
    case 0xA2: { // LIDATA
      unsigned Seg = R.index();
      uint32_t Offset = R.word(Is32);
      if (!CheckSeg(Seg))
        return Bad("bad data segment index");
      std::vector<uint8_t> Bytes;
      if ((Type & ~1) == OMF::LEDATA) {
        ArrayRef<uint8_t> Rest = R.rest();
        Bytes.assign(Rest.begin(), Rest.end());
      } else {
        while (R.more())
          if (!expandIteratedBlock(R, Is32, Bytes, 0))
            return Bad("bad LIDATA block");
      }
      Segment &S = Segments[Seg - 1];
      if (uint64_t(Offset) + Bytes.size() > S.Size)
        return Bad("data past the end of segment " + S.Name);
      if (S.Data.size() < S.Size)
        S.Data.resize(S.Size);
      std::copy(Bytes.begin(), Bytes.end(), S.Data.begin() + Offset);
      LastDataSeg = Seg - 1;
      LastDataOffset = Offset;
      LastDataIsComdat = false;
      break;
    }

    case OMF::COMDAT: { // and LCOMDAT
      uint8_t Flags = R.u8();
      uint8_t Attr = R.u8();
      R.u8();       // Alignment.
      R.word(Is32); // Offset of this piece.
      R.index();    // Type index.
      int32_t Seg = -1;
      if ((Attr & 0x0f) == 0) { // Explicit allocation: a public base.
        R.index(); // Group.
        unsigned SegIdx = R.index();
        if (SegIdx == 0)
          R.u16(); // Frame number.
        else if (!CheckSeg(SegIdx))
          return Bad("bad COMDAT segment");
        Seg = int32_t(SegIdx) - 1;
      }
      unsigned NameIdx = R.index();
      if (NameIdx == 0 || !CheckName(NameIdx))
        return Bad("bad COMDAT name index");
      // A continuation adds data to the COMDAT already defined.
      if (!(Flags & 0x01))
        Symbols.push_back({Names[NameIdx], Seg >= 0 ? SK_Defined : SK_Absolute,
                           bool(Flags & 0x04), Seg, 0, 0, false, true});
      LastDataIsComdat = true;
      break;
    }

    case OMF::FIXUPP:
      while (R.more()) {
        uint8_t B = R.u8();
        if (!(B & 0x80)) {
          // Thread subrecord.
          bool IsFrame = B & 0x40;
          uint8_t Method = (B >> 2) & 7;
          Thread &T = (IsFrame ? FrameThreads : TargetThreads)[B & 3];
          T.Valid = true;
          T.Method = Method;
          T.Index = (IsFrame ? Method < 3 : Method < 4) ? R.index() : 0;
          continue;
        }
        if (LastDataSeg < 0 && !LastDataIsComdat)
          return Bad("fixup with no data record before it");
        uint8_t Lo = R.u8();
        Relocation Rel;
        Rel.SegRelative = B & 0x40;
        Rel.LocType = (B >> 2) & 0xf;
        Rel.Offset = LastDataOffset + (((B & 3) << 8) | Lo);
        Rel.Symbol = -1;
        uint8_t FixData = R.u8();
        if (FixData & 0x80) {
          if (!FrameThreads[(FixData >> 4) & 3].Valid)
            return Bad("fixup uses an undefined frame thread");
        } else if (((FixData >> 4) & 7) < 3) {
          R.index(); // Frame datum.
        }
        uint8_t TargetMethod;
        unsigned TargetIndex;
        if (FixData & 0x08) {
          const Thread &T = TargetThreads[FixData & 3];
          if (!T.Valid)
            return Bad("fixup uses an undefined target thread");
          TargetMethod = T.Method & 3;
          TargetIndex = T.Index;
        } else {
          TargetMethod = FixData & 3;
          TargetIndex = R.index();
        }
        if (!(FixData & 0x04))
          R.word(Is32); // Target displacement.
        if (TargetMethod == OMF::T_External) {
          if (TargetIndex == 0 || TargetIndex >= Externals.size())
            return Bad("fixup to an undefined external index");
          Rel.Symbol = Externals[TargetIndex];
          Rel.TargetName = Symbols[Rel.Symbol].Name;
        } else if (TargetMethod == OMF::T_Segment) {
          if (!CheckSeg(TargetIndex))
            return Bad("fixup to an undefined segment");
          Rel.TargetName = Segments[TargetIndex - 1].Name;
        } else if (TargetMethod == OMF::T_Group) {
          if (TargetIndex == 0 || TargetIndex >= Groups.size())
            return Bad("fixup to an undefined group");
          Rel.TargetName = Groups[TargetIndex];
        }
        if (!LastDataIsComdat)
          Segments[LastDataSeg].Relocs.push_back(Rel);
      }
      break;

    case OMF::ALIAS:
      // An alias defines its name as another symbol, here or elsewhere.
      while (R.more()) {
        StringRef Name = R.name();
        StringRef Target = R.name();
        Aliases.push_back({Name, Target});
      }
      break;

    case OMF::MODEND:
      SeenEnd = true;
      break;

    default:
      // COMENT, LINNUM, BAKPAT and others carry nothing this reader needs.
      break;
    }

    if (R.failed())
      return Bad("truncated fields");
  }
  if (!SeenEnd)
    return malformed("no MODEND record");

  // An alias of a symbol this module defines is defined where it is, and
  // an alias of another an absolute symbol, which a linker resolves by
  // name.
  for (auto [Name, Target] : Aliases) {
    Symbol A = {Name, SK_Absolute, false, -1, 0, 0};
    for (const Symbol &T : Symbols)
      if (T.Name == Target && !T.Local &&
          (T.Kind == SK_Defined || T.Kind == SK_Absolute)) {
        A.Kind = T.Kind;
        A.Segment = T.Segment;
        A.Value = T.Value;
        A.FromComdat = T.FromComdat;
        break;
      }
    Symbols.push_back(A);
  }

  StringSet<> ComdatNames;
  for (const Symbol &Sym : Symbols)
    if (Sym.FromComdat)
      ComdatNames.insert(Sym.Name);
  for (Symbol &Sym : Symbols)
    if (Sym.Kind == SK_Undefined && ComdatNames.contains(Sym.Name))
      Sym.RefersToComdat = true;

  for (Segment &S : Segments)
    if (S.Data.size() < S.Size)
      S.Data.resize(S.Size);
  for (const Symbol &Sym : Symbols)
    if (Sym.Kind == SK_Defined && Sym.Value > Segments[Sym.Segment].Size)
      return malformed("symbol '" + Sym.Name + "' is past the end of its "
                       "segment");
  return Error::success();
}

// Symbols.

void OMFObjectFile::moveSymbolNext(DataRefImpl &Symb) const { ++Symb.d.a; }

Error OMFObjectFile::printSymbolName(raw_ostream &OS, DataRefImpl Symb) const {
  OS << Symbols[Symb.d.a].Name;
  return Error::success();
}

Expected<uint32_t> OMFObjectFile::getSymbolFlags(DataRefImpl Symb) const {
  const Symbol &S = Symbols[Symb.d.a];
  uint32_t Flags = S.Local ? 0 : uint32_t(SymbolRef::SF_Global);
  switch (S.Kind) {
  case SK_Defined:
    break;
  case SK_Undefined:
    Flags |= SymbolRef::SF_Undefined;
    if (S.RefersToComdat)
      Flags |= SymbolRef::SF_FormatSpecific;
    break;
  case SK_Common:
    Flags |= SymbolRef::SF_Common;
    break;
  case SK_Absolute:
    Flags |= SymbolRef::SF_Absolute;
    break;
  }
  return Flags;
}

basic_symbol_iterator OMFObjectFile::symbol_begin() const {
  DataRefImpl D;
  D.d.a = 0;
  return basic_symbol_iterator(SymbolRef(D, this));
}

basic_symbol_iterator OMFObjectFile::symbol_end() const {
  DataRefImpl D;
  D.d.a = Symbols.size();
  return basic_symbol_iterator(SymbolRef(D, this));
}

Expected<StringRef> OMFObjectFile::getSymbolName(DataRefImpl Symb) const {
  return Symbols[Symb.d.a].Name;
}

Expected<uint64_t> OMFObjectFile::getSymbolAddress(DataRefImpl Symb) const {
  return getSymbolValueImpl(Symb);
}

uint64_t OMFObjectFile::getSymbolValueImpl(DataRefImpl Symb) const {
  const Symbol &S = Symbols[Symb.d.a];
  return S.Kind == SK_Common ? S.CommonSize : S.Value;
}

uint64_t OMFObjectFile::getCommonSymbolSizeImpl(DataRefImpl Symb) const {
  return Symbols[Symb.d.a].CommonSize;
}

Expected<SymbolRef::Type>
OMFObjectFile::getSymbolType(DataRefImpl Symb) const {
  const Symbol &S = Symbols[Symb.d.a];
  switch (S.Kind) {
  case SK_Undefined:
    return SymbolRef::ST_Unknown;
  case SK_Common:
  case SK_Absolute:
    return SymbolRef::ST_Data;
  case SK_Defined:
    break;
  }
  DataRefImpl Sec;
  Sec.d.a = S.Segment;
  return isSectionText(Sec) ? SymbolRef::ST_Function : SymbolRef::ST_Data;
}

Expected<section_iterator>
OMFObjectFile::getSymbolSection(DataRefImpl Symb) const {
  const Symbol &S = Symbols[Symb.d.a];
  if (S.Kind != SK_Defined)
    return section_end();
  DataRefImpl Sec;
  Sec.d.a = S.Segment;
  return section_iterator(SectionRef(Sec, this));
}

// Sections.

void OMFObjectFile::moveSectionNext(DataRefImpl &Sec) const { ++Sec.d.a; }

Expected<StringRef> OMFObjectFile::getSectionName(DataRefImpl Sec) const {
  return Segments[Sec.d.a].Name;
}

uint64_t OMFObjectFile::getSectionSize(DataRefImpl Sec) const {
  return Segments[Sec.d.a].Size;
}

Expected<ArrayRef<uint8_t>>
OMFObjectFile::getSectionContents(DataRefImpl Sec) const {
  return ArrayRef<uint8_t>(Segments[Sec.d.a].Data);
}

uint64_t OMFObjectFile::getSectionAlignment(DataRefImpl Sec) const {
  uint8_t A = Segments[Sec.d.a].Align;
  return A ? A : 256;
}

bool OMFObjectFile::isSectionText(DataRefImpl Sec) const {
  return Segments[Sec.d.a].Class.ends_with_insensitive("CODE");
}

bool OMFObjectFile::isSectionBSS(DataRefImpl Sec) const {
  StringRef Class = Segments[Sec.d.a].Class;
  return Class.equals_insensitive("BSS") || Class.equals_insensitive("STACK");
}

bool OMFObjectFile::isSectionData(DataRefImpl Sec) const {
  return !isSectionText(Sec) && !isSectionBSS(Sec);
}

bool OMFObjectFile::isSectionVirtual(DataRefImpl Sec) const {
  return isSectionBSS(Sec);
}

relocation_iterator OMFObjectFile::section_rel_begin(DataRefImpl Sec) const {
  DataRefImpl R;
  R.d.a = Sec.d.a;
  R.d.b = 0;
  return relocation_iterator(RelocationRef(R, this));
}

relocation_iterator OMFObjectFile::section_rel_end(DataRefImpl Sec) const {
  DataRefImpl R;
  R.d.a = Sec.d.a;
  R.d.b = Segments[Sec.d.a].Relocs.size();
  return relocation_iterator(RelocationRef(R, this));
}

section_iterator OMFObjectFile::section_begin() const {
  DataRefImpl D;
  D.d.a = 0;
  return section_iterator(SectionRef(D, this));
}

section_iterator OMFObjectFile::section_end() const {
  DataRefImpl D;
  D.d.a = Segments.size();
  return section_iterator(SectionRef(D, this));
}

// Relocations.

void OMFObjectFile::moveRelocationNext(DataRefImpl &Rel) const { ++Rel.d.b; }

uint64_t OMFObjectFile::getRelocationOffset(DataRefImpl Rel) const {
  return Segments[Rel.d.a].Relocs[Rel.d.b].Offset;
}

symbol_iterator OMFObjectFile::getRelocationSymbol(DataRefImpl Rel) const {
  int32_t Sym = Segments[Rel.d.a].Relocs[Rel.d.b].Symbol;
  if (Sym < 0)
    return symbol_end();
  DataRefImpl D;
  D.d.a = Sym;
  return symbol_iterator(SymbolRef(D, this));
}

uint64_t OMFObjectFile::getRelocationType(DataRefImpl Rel) const {
  return relocationType(Segments[Rel.d.a].Relocs[Rel.d.b]);
}

void OMFObjectFile::getRelocationTypeName(
    DataRefImpl Rel, SmallVectorImpl<char> &Result) const {
  const Relocation &R = Segments[Rel.d.a].Relocs[Rel.d.b];
  static const char *const Names[16] = {
      "lobyte",   "offset16", "base16",   "ptr16:16",
      "hibyte",   "offset16(loader)", "?6", "?7",
      "?8",       "offset32", "?10",      "ptr16:32",
      "?12",      "offset32(loader)", "?14", "?15"};
  StringRef Name = Names[R.LocType];
  if (!R.SegRelative)
    Result.append({'s', 'e', 'l', 'f', ' '});
  Result.append(Name.begin(), Name.end());
}
