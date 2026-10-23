//===- lib/MC/MCOMFStreamer.cpp - OMF Object Output -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file assembles .s files and emits OMF .obj files. Inside MC the
// sections and symbols are ELF ones; OMFObjectWriter maps them to segments.
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCOMFStreamer.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

MCOMFStreamer::MCOMFStreamer(MCContext &Context,
                             std::unique_ptr<MCAsmBackend> MAB,
                             std::unique_ptr<MCObjectWriter> OW,
                             std::unique_ptr<MCCodeEmitter> Emitter)
    : MCObjectStreamer(Context, std::move(MAB), std::move(OW),
                       std::move(Emitter)) {}

MCOMFStreamer::~MCOMFStreamer() = default;

void MCOMFStreamer::changeSection(MCSection *Section, uint32_t Subsection) {
  MCObjectStreamer::changeSection(Section, Subsection);
  auto *Sym = static_cast<MCSymbolELF *>(Section->getBeginSymbol());
  Sym->setBinding(ELF::STB_LOCAL);
  Sym->setType(ELF::STT_SECTION);
}

bool MCOMFStreamer::emitSymbolAttribute(MCSymbol *S, MCSymbolAttr Attribute) {
  auto *Symbol = static_cast<MCSymbolELF *>(S);
  switch (Attribute) {
  case MCSA_Global:
  case MCSA_Extern:
  // OMF has weak externals (WKEXT), but they are not emitted yet. A weak
  // symbol is treated as a strong one.
  case MCSA_Weak:
  case MCSA_WeakReference:
    getAssembler().registerSymbol(*Symbol);
    Symbol->setBinding(ELF::STB_GLOBAL);
    return true;
  case MCSA_Local:
    getAssembler().registerSymbol(*Symbol);
    Symbol->setBinding(ELF::STB_LOCAL);
    return true;
  // Symbol types and visibility have no OMF meaning.
  case MCSA_ELF_TypeFunction:
  case MCSA_ELF_TypeObject:
  case MCSA_ELF_TypeNoType:
  case MCSA_ELF_TypeCommon:
  case MCSA_Hidden:
  case MCSA_Internal:
  case MCSA_Protected:
  case MCSA_NoDeadStrip:
    return true;
  default:
    return false;
  }
}

void MCOMFStreamer::emitCommonSymbol(MCSymbol *S, uint64_t Size,
                                     Align ByteAlignment) {
  auto *Symbol = static_cast<MCSymbolELF *>(S);
  getAssembler().registerSymbol(*Symbol);
  if (!Symbol->isBindingSet())
    Symbol->setBinding(ELF::STB_GLOBAL);

  if (Symbol->getBinding() == ELF::STB_LOCAL) {
    // A local common symbol is plain zero-initialized data.
    MCSectionSubPair P = getCurrentSection();
    switchSection(getContext().getObjectFileInfo()->getBSSSection());
    emitValueToAlignment(ByteAlignment, 0, 1, 0);
    emitLabel(Symbol);
    emitZeros(Size);
    switchSection(P.first, P.second);
    return;
  }

  // A global common symbol becomes a COMDEF (near communal variable).
  if (Symbol->declareCommon(Size, ByteAlignment))
    getContext().reportError(SMLoc(), "symbol '" + Symbol->getName() +
                                          "' redeclared as different type");
}

void MCOMFStreamer::emitLocalCommonSymbol(MCSymbol *S, uint64_t Size,
                                          Align ByteAlignment) {
  auto *Symbol = static_cast<MCSymbolELF *>(S);
  getAssembler().registerSymbol(*Symbol);
  Symbol->setBinding(ELF::STB_LOCAL);
  emitCommonSymbol(Symbol, Size, ByteAlignment);
}

MCStreamer *llvm::createOMFStreamer(MCContext &Context,
                                    std::unique_ptr<MCAsmBackend> &&MAB,
                                    std::unique_ptr<MCObjectWriter> &&OW,
                                    std::unique_ptr<MCCodeEmitter> &&CE) {
  return new MCOMFStreamer(Context, std::move(MAB), std::move(OW),
                           std::move(CE));
}
