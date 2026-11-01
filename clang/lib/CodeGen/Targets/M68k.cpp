//===- M68k.cpp -----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"
#include "llvm/Support/Format.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// M68k ABI Implementation
//===----------------------------------------------------------------------===//

namespace {

/// The C convention is the default one. Pascal functions (classic Mac OS)
/// take records of up to 4 bytes by value as integers, larger ones by
/// address, and return results of up to 4 bytes.
class M68kABIInfo : public DefaultABIInfo {
public:
  M68kABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  void computeInfo(CGFunctionInfo &FI) const override {
    if (FI.getCallingConvention() != llvm::CallingConv::M68k_Pascal) {
      DefaultABIInfo::computeInfo(FI);
      return;
    }
    if (!getCXXABI().classifyReturnType(FI))
      FI.getReturnInfo() = classifyPascalType(FI.getReturnType(), true);
    for (auto &I : FI.arguments())
      I.info = classifyPascalType(I.type, false);
  }

private:
  ABIArgInfo classifyPascalType(QualType Ty, bool IsReturn) const {
    if (Ty->isVoidType())
      return ABIArgInfo::getIgnore();
    if (!isAggregateTypeForABI(Ty))
      return IsReturn ? DefaultABIInfo::classifyReturnType(Ty)
                      : DefaultABIInfo::classifyArgumentType(Ty);
    uint64_t Size = getContext().getTypeSize(Ty);
    if (Size == 8 || Size == 16 || Size == 32)
      return ABIArgInfo::getDirect(
          llvm::IntegerType::get(getVMContext(), Size));
    if (IsReturn)
      return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace());
    // Larger records go by address; the callee copies them if it needs to.
    return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                   /*ByVal=*/false);
  }
};

class M68kTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  M68kTargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<M68kABIInfo>(CGT)) {}
  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &M) const override;
};

} // namespace

void M68kTargetCodeGenInfo::setTargetAttributes(
    const Decl *D, llvm::GlobalValue *GV, CodeGen::CodeGenModule &M) const {
  if (const auto *FD = dyn_cast_or_null<FunctionDecl>(D)) {
    // A Toolbox trap: calls emit these code words instead of a jsr.
    if (const auto *RI = FD->getAttr<M68kRawInlineAttr>()) {
      std::string Words;
      llvm::raw_string_ostream OS(Words);
      for (unsigned W : RI->words()) {
        if (!Words.empty())
          OS << ',';
        OS << llvm::format_hex_no_prefix(W, 4, /*Upper=*/true);
      }
      cast<llvm::Function>(GV)->addFnAttr("m68k-inline", Words);
    }
    if (const auto *attr = FD->getAttr<M68kInterruptAttr>()) {
      // Handle 'interrupt' attribute:
      llvm::Function *F = cast<llvm::Function>(GV);

      // Step 1: Set ISR calling convention.
      F->setCallingConv(llvm::CallingConv::M68k_INTR);

      // Step 2: Add attributes goodness.
      F->addFnAttr(llvm::Attribute::NoInline);

      // Step 3: Emit ISR vector alias.
      unsigned Num = attr->getNumber() / 2;
      llvm::GlobalAlias::create(llvm::Function::ExternalLinkage,
                                "__isr_" + Twine(Num), F);
    }
  }
}

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createM68kTargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<M68kTargetCodeGenInfo>(CGM.getTypes());
}
