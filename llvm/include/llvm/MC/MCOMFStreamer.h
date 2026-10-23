//===- MCOMFStreamer.h - MCStreamer OMF Object File Interface ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCOMFSTREAMER_H
#define LLVM_MC_MCOMFSTREAMER_H

#include "llvm/MC/MCObjectStreamer.h"
#include "llvm/Support/Compiler.h"

namespace llvm {

/// Streamer for OMF objects. Sections and symbols are the ELF ones, so the
/// ELF assembly parser and section names work unchanged.
class LLVM_ABI MCOMFStreamer : public MCObjectStreamer {
public:
  MCOMFStreamer(MCContext &Context, std::unique_ptr<MCAsmBackend> MAB,
                std::unique_ptr<MCObjectWriter> OW,
                std::unique_ptr<MCCodeEmitter> Emitter);
  ~MCOMFStreamer() override;

  void changeSection(MCSection *Section, uint32_t Subsection = 0) override;
  bool emitSymbolAttribute(MCSymbol *Symbol, MCSymbolAttr Attribute) override;
  void emitCommonSymbol(MCSymbol *Symbol, uint64_t Size,
                        Align ByteAlignment) override;
  void emitLocalCommonSymbol(MCSymbol *Symbol, uint64_t Size,
                             Align ByteAlignment) override;
};

} // end namespace llvm

#endif // LLVM_MC_MCOMFSTREAMER_H
