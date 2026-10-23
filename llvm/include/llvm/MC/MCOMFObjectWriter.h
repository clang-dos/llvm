//===- MCOMFObjectWriter.h - OMF Object Writer ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The OMF writer reuses ELF-style sections and symbols inside MC. At write
// time each allocatable section is mapped onto a classic DOS segment
// (_TEXT, CONST, _DATA, _BSS, STACK), and data segments are put in DGROUP.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCOMFOBJECTWRITER_H
#define LLVM_MC_MCOMFOBJECTWRITER_H

#include "llvm/MC/MCObjectWriter.h"
#include "llvm/Support/Compiler.h"
#include <memory>

namespace llvm {
class raw_pwrite_stream;

class LLVM_ABI MCOMFObjectTargetWriter : public MCObjectTargetWriter {
  bool CodeInDGROUP;

public:
  /// \p CodeInDGROUP puts _TEXT in DGROUP as well, as the tiny memory model
  /// needs (CS = DS = SS).
  explicit MCOMFObjectTargetWriter(bool CodeInDGROUP)
      : CodeInDGROUP(CodeInDGROUP) {}
  ~MCOMFObjectTargetWriter() override;

  Triple::ObjectFormatType getFormat() const override { return Triple::OMF; }
  bool isCodeInDGROUP() const { return CodeInDGROUP; }

  static bool classof(const MCObjectTargetWriter *W) {
    return W->getFormat() == Triple::OMF;
  }
};

/// Construct a new OMF writer instance.
LLVM_ABI std::unique_ptr<MCObjectWriter>
createOMFObjectWriter(std::unique_ptr<MCOMFObjectTargetWriter> MOTW,
                      raw_pwrite_stream &OS);

} // end namespace llvm

#endif // LLVM_MC_MCOMFOBJECTWRITER_H
