//===-- X86TargetObjectFile.cpp - X86 Object Info -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "X86TargetObjectFile.h"
#include "MCTargetDesc/X86MCAsmInfo.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;
using namespace dwarf;

const MCExpr *X86_64MachoTargetObjectFile::getTTypeGlobalReference(
    const GlobalValue *GV, unsigned Encoding, const TargetMachine &TM,
    MachineModuleInfo *MMI, MCStreamer &Streamer) const {

  // On Darwin/X86-64, we can reference dwarf symbols with foo@GOTPCREL+4, which
  // is an indirect pc-relative reference.
  if ((Encoding & DW_EH_PE_indirect) && (Encoding & DW_EH_PE_pcrel)) {
    const MCSymbol *Sym = TM.getSymbol(GV);
    const MCExpr *Res =
        MCSymbolRefExpr::create(Sym, X86::S_GOTPCREL, getContext());
    const MCExpr *Four = MCConstantExpr::create(4, getContext());
    return MCBinaryExpr::createAdd(Res, Four, getContext());
  }

  return TargetLoweringObjectFileMachO::getTTypeGlobalReference(
      GV, Encoding, TM, MMI, Streamer);
}

MCSymbol *X86_64MachoTargetObjectFile::getCFIPersonalitySymbol(
    const GlobalValue *GV, const TargetMachine &TM,
    MachineModuleInfo *MMI) const {
  return TM.getSymbol(GV);
}

const MCExpr *X86_64MachoTargetObjectFile::getIndirectSymViaGOTPCRel(
    const GlobalValue *GV, const MCSymbol *Sym, const MCValue &MV,
    int64_t Offset, MachineModuleInfo *MMI, MCStreamer &Streamer) const {
  // On Darwin/X86-64, we need to use foo@GOTPCREL+4 to access the got entry
  // from a data section. In case there's an additional offset, then use
  // foo@GOTPCREL+4+<offset>.
  unsigned FinalOff = Offset+MV.getConstant()+4;
  const MCExpr *Res =
      MCSymbolRefExpr::create(Sym, X86::S_GOTPCREL, getContext());
  const MCExpr *Off = MCConstantExpr::create(FinalOff, getContext());
  return MCBinaryExpr::createAdd(Res, Off, getContext());
}

X86ELFTargetObjectFile::X86ELFTargetObjectFile() {
  PLTRelativeSpecifier = X86::S_PLT;
}

const MCExpr *X86ELFTargetObjectFile::getDebugThreadLocalSymbol(
    const MCSymbol *Sym) const {
  return MCSymbolRefExpr::create(Sym, X86::S_DTPOFF, getContext());
}

const MCExpr *X86_64ELFTargetObjectFile::getIndirectSymViaGOTPCRel(
    const GlobalValue *GV, const MCSymbol *Sym, const MCValue &MV,
    int64_t Offset, MachineModuleInfo *MMI, MCStreamer &Streamer) const {
  int64_t FinalOffset = Offset + MV.getConstant();
  const MCExpr *Res =
      MCSymbolRefExpr::create(Sym, X86::S_GOTPCREL, getContext());
  const MCExpr *Off = MCConstantExpr::create(FinalOffset, getContext());
  return MCBinaryExpr::createAdd(Res, Off, getContext());
}

/// The section of an i8086 variable outside DGROUP, or null for one in it.
static MCSection *getFarDataSection(MCContext &Ctx, const GlobalObject *GO,
                                    SectionKind Kind,
                                    const TargetMachine &TM) {
  const auto *GV = dyn_cast<GlobalVariable>(GO);
  if (!GV)
    return nullptr;
  unsigned AS = GV->getAddressSpace();
  if (AS != 2 && AS != 3)
    return nullptr;
  StringRef Prefix = Kind.isBSS()        ? ".farbss."
                     : Kind.isReadOnly() ? ".farrodata."
                                         : ".fardata.";
  unsigned Type = Kind.isBSS() ? ELF::SHT_NOBITS : ELF::SHT_PROGBITS;
  unsigned Flags = ELF::SHF_ALLOC;
  if (!Kind.isReadOnly())
    Flags |= ELF::SHF_WRITE;
  return Ctx.getELFSection(Prefix + TM.getSymbol(GO)->getName(), Type, Flags);
}

MCSection *X86OMFTargetObjectFile::SelectSectionForGlobal(
    const GlobalObject *GO, SectionKind Kind, const TargetMachine &TM) const {
  if (MCSection *S = getFarDataSection(getContext(), GO, Kind, TM))
    return S;
  return X86ELFTargetObjectFile::SelectSectionForGlobal(GO, Kind, TM);
}

MCSection *X86OMFTargetObjectFile::getExplicitSectionGlobal(
    const GlobalObject *GO, SectionKind Kind, const TargetMachine &TM) const {
  // A far variable stays in a segment of its own whatever section it names.
  if (MCSection *S = getFarDataSection(getContext(), GO, Kind, TM))
    return S;
  return X86ELFTargetObjectFile::getExplicitSectionGlobal(GO, Kind, TM);
}
