//===-- PPCMachObjectWriter.cpp - PPC Mach-O Writer -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Relocations for 32-bit PowerPC Mach-O, as read by Apple's classic ld
// (cctools) and by ld64. The rules follow Apple's assembler:
//
//  * hi16/lo16/ha16 relocations name the start of the instruction, and are
//    always followed by a PPC_RELOC_PAIR entry whose r_address holds the
//    other 16 bits of the full value, so the linker can rebuild it.
//  * Differences (a - b) use scattered *_SECTDIFF entries whose PAIR carries
//    the address of b.
//  * A reference to a local symbol plus an offset uses a scattered entry, so
//    the linker can tell which symbol was meant when it moves code around.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/PPCFixupKinds.h"
#include "MCTargetDesc/PPCMCAsmInfo.h"
#include "MCTargetDesc/PPCMCTargetDesc.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCMachObjectWriter.h"
#include "llvm/MC/MCSectionMachO.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"

using namespace llvm;

namespace {
class PPCMachObjectWriter : public MCMachObjectTargetWriter {
public:
  PPCMachObjectWriter(bool Is64Bit, uint32_t CPUType, uint32_t CPUSubtype)
      : MCMachObjectTargetWriter(Is64Bit, CPUType, CPUSubtype) {}

  void recordRelocation(MachObjectWriter *Writer, MCAssembler &Asm,
                        const MCFragment *Fragment, const MCFixup &Fixup,
                        MCValue Target, uint64_t &FixedValue) override;

private:
  void recordScattered(MachObjectWriter *Writer, const MCAssembler &Asm,
                       const MCFragment *Fragment, const MCFixup &Fixup,
                       MCValue Target, unsigned Type, unsigned Log2Size,
                       uint64_t &FixedValue);
};
} // namespace

/// r_length of the relocation (log2 of the bytes it covers).
static void addReloc(MachObjectWriter *Writer, const MCSymbol *RelSymbol,
                     const MCSection *Sec, MachO::any_relocation_info MRE) {
  Writer->addRelocation(RelSymbol, Sec, MRE);
}

static bool getLog2Size(unsigned Kind, unsigned &Log2Size) {
  switch (Kind) {
  default:
    return false;
  case FK_Data_1:
    Log2Size = 0;
    return true;
  case FK_Data_2:
    Log2Size = 1;
    return true;
  case FK_Data_4:
  case PPC::fixup_ppc_br24:
  case PPC::fixup_ppc_brcond14:
  case PPC::fixup_ppc_half16:
    // hi16/lo16/ha16 relocations cover the whole instruction.
    Log2Size = 2;
    return true;
  }
}

/// Is this a relocation that is followed by a PAIR carrying the other half?
static bool isHalfType(unsigned Type) {
  switch (Type) {
  case MachO::PPC_RELOC_HI16:
  case MachO::PPC_RELOC_LO16:
  case MachO::PPC_RELOC_HA16:
  case MachO::PPC_RELOC_HI16_SECTDIFF:
  case MachO::PPC_RELOC_LO16_SECTDIFF:
  case MachO::PPC_RELOC_HA16_SECTDIFF:
    return true;
  default:
    return false;
  }
}

/// Split \p Full into the half that goes in the instruction (returned in
/// \p Full) and the half that goes in the PAIR entry (the return value).
static uint32_t splitHalves(unsigned Type, uint64_t &Full) {
  uint32_t V = uint32_t(Full);
  switch (Type) {
  case MachO::PPC_RELOC_LO16:
  case MachO::PPC_RELOC_LO16_SECTDIFF:
    Full = V & 0xffff;
    return (V >> 16) & 0xffff;
  case MachO::PPC_RELOC_HI16:
  case MachO::PPC_RELOC_HI16_SECTDIFF:
    Full = (V >> 16) & 0xffff;
    return V & 0xffff;
  case MachO::PPC_RELOC_HA16:
  case MachO::PPC_RELOC_HA16_SECTDIFF:
    Full = ((V + 0x8000) >> 16) & 0xffff;
    return V & 0xffff;
  default:
    llvm_unreachable("not a half relocation");
  }
}

/// A non-scattered relocation_info. On a big-endian target the C bitfields
/// are laid out from the most significant bit: r_symbolnum:24, r_pcrel:1,
/// r_length:2, r_extern:1, r_type:4.
static MachO::any_relocation_info makeReloc(uint32_t Address, uint32_t Index,
                                            unsigned IsPCRel, unsigned Log2Size,
                                            unsigned IsExtern, unsigned Type) {
  MachO::any_relocation_info MRE;
  MRE.r_word0 = Address;
  MRE.r_word1 = (Index << 8) | (IsPCRel << 7) | (Log2Size << 5) |
                (IsExtern << 4) | (Type << 0);
  return MRE;
}

/// A scattered_relocation_info. Its layout is the same on both endians.
static MachO::any_relocation_info makeScattered(uint32_t Address, unsigned Type,
                                                unsigned Log2Size,
                                                unsigned IsPCRel,
                                                uint32_t Value) {
  MachO::any_relocation_info MRE;
  MRE.r_word0 = (Address & 0xffffff) | (Type << 24) | (Log2Size << 28) |
                (IsPCRel << 30) | MachO::R_SCATTERED;
  MRE.r_word1 = Value;
  return MRE;
}

/// The address the relocation names. A hi16/lo16/ha16 relocation names the
/// start of the instruction rather than the halfword that the fixup patches.
static uint32_t getFixupAddress(const MCAssembler &Asm,
                                const MCFragment *Fragment,
                                const MCFixup &Fixup) {
  uint32_t Offset = Asm.getFragmentOffset(*Fragment) + Fixup.getOffset();
  if (Fixup.getKind() == PPC::fixup_ppc_half16)
    Offset &= ~uint32_t(3);
  return Offset;
}

void PPCMachObjectWriter::recordScattered(MachObjectWriter *Writer,
                                          const MCAssembler &Asm,
                                          const MCFragment *Fragment,
                                          const MCFixup &Fixup, MCValue Target,
                                          unsigned Type, unsigned Log2Size,
                                          uint64_t &FixedValue) {
  uint32_t Address = getFixupAddress(Asm, Fragment, Fixup);
  unsigned IsPCRel = Fixup.isPCRel();
  if (Address > 0xffffff) {
    reportError(Fixup.getLoc(),
                "section too large, can't encode r_address (0x" +
                    utohexstr(Address) +
                    ") into 24 bits of a scattered relocation entry");
    return;
  }

  const MCSymbol *A = Target.getAddSym();
  if (!A->getFragment()) {
    reportError(Fixup.getLoc(),
                "symbol '" + A->getName() +
                    "' can not be undefined in a subtraction expression");
    return;
  }
  uint32_t Value = Writer->getSymbolAddress(*A);
  FixedValue += Writer->getSectionAddress(A->getFragment()->getParent());

  uint32_t Value2 = 0;
  const MCSymbol *B = Target.getSubSym();
  if (B) {
    if (!B->getFragment()) {
      reportError(Fixup.getLoc(),
                  "symbol '" + B->getName() +
                      "' can not be undefined in a subtraction expression");
      return;
    }
    Value2 = Writer->getSymbolAddress(*B);
    FixedValue -= Writer->getSectionAddress(B->getFragment()->getParent());
    switch (Type) {
    case MachO::PPC_RELOC_VANILLA:
      Type = MachO::PPC_RELOC_SECTDIFF;
      break;
    case MachO::PPC_RELOC_HI16:
      Type = MachO::PPC_RELOC_HI16_SECTDIFF;
      break;
    case MachO::PPC_RELOC_LO16:
      Type = MachO::PPC_RELOC_LO16_SECTDIFF;
      break;
    case MachO::PPC_RELOC_HA16:
      Type = MachO::PPC_RELOC_HA16_SECTDIFF;
      break;
    default:
      reportError(Fixup.getLoc(), "unsupported symbol difference");
      return;
    }
  }

  // Relocations are written out in reverse order, so the PAIR comes first.
  if (isHalfType(Type)) {
    uint32_t OtherHalf = splitHalves(Type, FixedValue);
    if (B)
      addReloc(Writer, nullptr, Fragment->getParent(),
               makeScattered(OtherHalf, MachO::PPC_RELOC_PAIR, Log2Size,
                             IsPCRel, Value2));
    else
      addReloc(Writer, nullptr, Fragment->getParent(),
               makeReloc(OtherHalf, 0, 0, Log2Size, 0, MachO::PPC_RELOC_PAIR));
  } else if (B) {
    addReloc(
        Writer, nullptr, Fragment->getParent(),
        makeScattered(0, MachO::PPC_RELOC_PAIR, Log2Size, IsPCRel, Value2));
  }

  addReloc(Writer, nullptr, Fragment->getParent(),
           makeScattered(Address, Type, Log2Size, IsPCRel, Value));
}

void PPCMachObjectWriter::recordRelocation(MachObjectWriter *Writer,
                                           MCAssembler &Asm,
                                           const MCFragment *Fragment,
                                           const MCFixup &Fixup, MCValue Target,
                                           uint64_t &FixedValue) {
  if (Writer->is64Bit()) {
    reportError(Fixup.getLoc(), "64-bit PowerPC Mach-O is not supported");
    return;
  }

  unsigned Kind = Fixup.getKind();
  unsigned Log2Size;
  if (!getLog2Size(Kind, Log2Size)) {
    reportError(Fixup.getLoc(), "unsupported relocation in Mach-O");
    return;
  }

  unsigned IsPCRel = Fixup.isPCRel();
  unsigned Type = MachO::PPC_RELOC_VANILLA;
  switch (Kind) {
  case PPC::fixup_ppc_br24:
    Type = MachO::PPC_RELOC_BR24;
    break;
  case PPC::fixup_ppc_brcond14:
    Type = MachO::PPC_RELOC_BR14;
    break;
  case PPC::fixup_ppc_half16:
    switch (Target.getSpecifier()) {
    case PPC::S_LO:
      Type = MachO::PPC_RELOC_LO16;
      break;
    case PPC::S_HI:
      Type = MachO::PPC_RELOC_HI16;
      break;
    case PPC::S_HA:
      Type = MachO::PPC_RELOC_HA16;
      break;
    default:
      reportError(Fixup.getLoc(),
                  "16-bit relocation needs lo16(), hi16() or ha16()");
      return;
    }
    break;
  default:
    if (Target.getSpecifier()) {
      reportError(Fixup.getLoc(), "unsupported relocation specifier");
      return;
    }
    break;
  }

  // Differences always need scattered relocations.
  if (Target.getSubSym()) {
    if (IsPCRel) {
      reportError(Fixup.getLoc(), "unsupported pc-relative difference");
      return;
    }
    recordScattered(Writer, Asm, Fragment, Fixup, Target, Type, Log2Size,
                    FixedValue);
    return;
  }

  const MCSymbol *A = Target.getAddSym();
  if (!A) {
    // A plain constant. Only the halves need a relocation, and only if the
    // assembler kept the specifier; there is nothing for the linker to do.
    if (isHalfType(Type))
      splitHalves(Type, FixedValue);
    return;
  }

  // Resolve constant variables.
  if (A->isVariable()) {
    MCValue Val;
    if (A->getVariableValue()->evaluateAsRelocatable(Val, &Asm) &&
        Val.isAbsolute()) {
      FixedValue = Val.getConstant();
      if (isHalfType(Type))
        splitHalves(Type, FixedValue);
      return;
    }
  }

  // A local symbol plus an offset needs a scattered relocation, so the linker
  // knows which symbol (and therefore which block of code) was meant.
  bool IsExtern = Writer->doesSymbolRequireExternRelocation(*A);
  if (!IsExtern && !IsPCRel && Target.getConstant() != 0 &&
      getFixupAddress(Asm, Fragment, Fixup) <= 0xffffff) {
    recordScattered(Writer, Asm, Fragment, Fixup, Target, Type, Log2Size,
                    FixedValue);
    return;
  }

  uint32_t Address = getFixupAddress(Asm, Fragment, Fixup);
  unsigned Index = 0;
  const MCSymbol *RelSymbol = nullptr;
  if (IsExtern) {
    RelSymbol = A;
    // The value holds only the addend. Take back the symbol's offset that was
    // added if it is defined here (a weak definition, for example).
    if (!A->isUndefined())
      FixedValue -= Asm.getSymbolOffset(*A);
  } else {
    // Section ordinals in relocations are 1-based.
    const MCSection &Sec = A->getSection();
    Index = Sec.getOrdinal() + 1;
    FixedValue += Writer->getSectionAddress(&Sec);
  }
  if (IsPCRel)
    FixedValue -= Writer->getSectionAddress(Fragment->getParent());

  // Relocations are written out in reverse order, so the PAIR comes first.
  if (isHalfType(Type)) {
    uint32_t OtherHalf = splitHalves(Type, FixedValue);
    addReloc(Writer, nullptr, Fragment->getParent(),
             makeReloc(OtherHalf, 0, 0, Log2Size, 0, MachO::PPC_RELOC_PAIR));
  }
  addReloc(Writer, RelSymbol, Fragment->getParent(),
           makeReloc(Address, Index, IsPCRel, Log2Size,
                     /*IsExtern=*/0, Type));
}

std::unique_ptr<MCObjectTargetWriter>
llvm::createPPCMachObjectWriter(bool Is64Bit, uint32_t CPUType,
                                uint32_t CPUSubtype) {
  return std::make_unique<PPCMachObjectWriter>(Is64Bit, CPUType, CPUSubtype);
}
