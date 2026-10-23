//===- OMFObjectFile.h - OMF object file implementation ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A reader for Intel/Microsoft OMF (Relocatable Object Module Format) objects,
// the 16-bit DOS object format. Segments are presented as sections, PUBDEF,
// EXTDEF and COMDEF entries as symbols, and FIXUPP entries as relocations.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_OBJECT_OMFOBJECTFILE_H
#define LLVM_OBJECT_OMFOBJECTFILE_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/SubtargetFeature.h"
#include <vector>

namespace llvm {
namespace object {

class LLVM_ABI OMFObjectFile : public ObjectFile {
public:
  struct Relocation {
    uint32_t Offset;  // Location within the segment.
    uint8_t LocType;  // OMF::LocationType.
    bool SegRelative; // False means self-relative.
    // Target: an external symbol (index into Symbols), or a segment or group.
    int32_t Symbol;   // -1 if the target is not a symbol.
    StringRef TargetName; // The symbol, segment or group name.
  };

  struct Segment {
    StringRef Name;
    StringRef Class;
    StringRef Group; // Empty if the segment is in no group.
    uint32_t Size = 0;
    uint8_t Align = 1; // In bytes.
    uint8_t Combine = 0;
    bool Use32 = false;
    std::vector<uint8_t> Data;
    std::vector<Relocation> Relocs;
  };

  enum SymbolKind { SK_Defined, SK_Undefined, SK_Common, SK_Absolute };

  struct Symbol {
    StringRef Name;
    SymbolKind Kind;
    bool Local;       // LPUBDEF or LEXTDEF.
    int32_t Segment;  // Index into Segments, or -1.
    uint32_t Value;   // Offset within the segment, or the absolute value.
    uint32_t CommonSize;
    // An EXTDEF of a COMDAT defined in the same module: listed as format
    // specific, since the module defines the name.
    bool RefersToComdat = false;
    bool FromComdat = false; // Defined by a COMDAT record.
  };

  static Expected<std::unique_ptr<OMFObjectFile>> create(MemoryBufferRef Object);

  static bool classof(const Binary *V) { return V->isOMF(); }

  StringRef getModuleName() const { return ModuleName; }
  ArrayRef<Segment> segments() const { return Segments; }
  ArrayRef<Symbol> symbols_list() const { return Symbols; }

  // SymbolicFile and ObjectFile interface.
  void moveSymbolNext(DataRefImpl &Symb) const override;
  Error printSymbolName(raw_ostream &OS, DataRefImpl Symb) const override;
  Expected<uint32_t> getSymbolFlags(DataRefImpl Symb) const override;
  basic_symbol_iterator symbol_begin() const override;
  basic_symbol_iterator symbol_end() const override;
  bool is64Bit() const override { return false; }

  Expected<StringRef> getSymbolName(DataRefImpl Symb) const override;
  Expected<uint64_t> getSymbolAddress(DataRefImpl Symb) const override;
  uint64_t getSymbolValueImpl(DataRefImpl Symb) const override;
  uint64_t getCommonSymbolSizeImpl(DataRefImpl Symb) const override;
  Expected<SymbolRef::Type> getSymbolType(DataRefImpl Symb) const override;
  Expected<section_iterator> getSymbolSection(DataRefImpl Symb) const override;

  void moveSectionNext(DataRefImpl &Sec) const override;
  Expected<StringRef> getSectionName(DataRefImpl Sec) const override;
  uint64_t getSectionAddress(DataRefImpl Sec) const override { return 0; }
  uint64_t getSectionIndex(DataRefImpl Sec) const override { return Sec.d.a; }
  uint64_t getSectionSize(DataRefImpl Sec) const override;
  Expected<ArrayRef<uint8_t>>
  getSectionContents(DataRefImpl Sec) const override;
  uint64_t getSectionAlignment(DataRefImpl Sec) const override;
  bool isSectionCompressed(DataRefImpl Sec) const override { return false; }
  bool isSectionText(DataRefImpl Sec) const override;
  bool isSectionData(DataRefImpl Sec) const override;
  bool isSectionBSS(DataRefImpl Sec) const override;
  bool isSectionVirtual(DataRefImpl Sec) const override;
  relocation_iterator section_rel_begin(DataRefImpl Sec) const override;
  relocation_iterator section_rel_end(DataRefImpl Sec) const override;

  void moveRelocationNext(DataRefImpl &Rel) const override;
  uint64_t getRelocationOffset(DataRefImpl Rel) const override;
  symbol_iterator getRelocationSymbol(DataRefImpl Rel) const override;
  uint64_t getRelocationType(DataRefImpl Rel) const override;
  void getRelocationTypeName(DataRefImpl Rel,
                             SmallVectorImpl<char> &Result) const override;

  section_iterator section_begin() const override;
  section_iterator section_end() const override;

  uint8_t getBytesInAddress() const override { return 2; }
  StringRef getFileFormatName() const override { return "OMF-16"; }
  Triple::ArchType getArch() const override { return Triple::i8086; }
  Expected<SubtargetFeatures> getFeatures() const override {
    return SubtargetFeatures();
  }
  bool isRelocatableObject() const override { return true; }

  /// The relocation type number: the OMF location type, plus 0x100 for a
  /// self-relative fixup.
  static uint64_t relocationType(const Relocation &R) {
    return R.LocType | (R.SegRelative ? 0 : 0x100);
  }

private:
  OMFObjectFile(MemoryBufferRef Object) : ObjectFile(ID_OMF, Object) {}
  Error parse();

  StringRef ModuleName;
  std::vector<Segment> Segments;
  std::vector<Symbol> Symbols;
};

} // end namespace object
} // end namespace llvm

#endif // LLVM_OBJECT_OMFOBJECTFILE_H
