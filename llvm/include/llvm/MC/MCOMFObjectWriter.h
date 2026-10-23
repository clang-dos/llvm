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
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Compiler.h"
#include <memory>

namespace llvm {
class raw_pwrite_stream;

class LLVM_ABI MCOMFObjectTargetWriter : public MCObjectTargetWriter {
  bool CodeInDGROUP;
  bool FarCode;

public:
  /// \p CodeInDGROUP puts _TEXT in DGROUP as well, as the tiny memory model
  /// needs (CS = DS = SS). \p FarCode gives each module a code segment of
  /// its own, <module>_TEXT, as the memory models with far calls do.
  explicit MCOMFObjectTargetWriter(bool CodeInDGROUP, bool FarCode = false)
      : CodeInDGROUP(CodeInDGROUP), FarCode(FarCode) {}
  ~MCOMFObjectTargetWriter() override;

  Triple::ObjectFormatType getFormat() const override { return Triple::OMF; }
  bool isCodeInDGROUP() const { return CodeInDGROUP; }
  bool hasFarCode() const { return FarCode; }

  /// Whether \p Target asks for the segment (paragraph) of a symbol rather
  /// than its offset, as `sym@SEG` does on X86. Such a fixup is a base16
  /// fixup, which the DOS loader relocates.
  virtual bool isSegmentReference(const MCValue &Target) const { return false; }

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
