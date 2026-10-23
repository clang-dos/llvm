//===- X86HugePointers.cpp - i8086 huge pointer arithmetic ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exceptions
//
//===----------------------------------------------------------------------===//
//
// i8086 huge pointers (address space 3) are 32-bit segment:offset pairs that
// address objects larger than 64 KiB. Their value as an integer is the linear
// address, segment * 16 + offset, so ptrtoint, ptrtoaddr, inttoptr and
// comparisons convert to and from it, and pointer arithmetic carries into the
// segment.
// Arithmetic results are normalized: the offset is 0 to 15. Other huge
// pointers, such as a near object cast to huge, may have any offset.
//
// After this pass the code generator treats a huge pointer as the raw pair,
// the same as a far pointer: loads and stores go through the segment.
//
//===----------------------------------------------------------------------===//

#include "X86.h"
#include "llvm/Analysis/Utils/Local.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

#define DEBUG_TYPE "x86-huge-pointers"

static constexpr unsigned HugeAS = 3;

static bool isHugePtr(Type *T) {
  return T->isPointerTy() && T->getPointerAddressSpace() == HugeAS;
}

namespace {
class HugePointers {
  const DataLayout &DL;
  LLVMContext &Ctx;
  Type *I16, *I32;

public:
  explicit HugePointers(Function &F)
      : DL(F.getDataLayout()), Ctx(F.getContext()),
        I16(Type::getInt16Ty(Ctx)), I32(Type::getInt32Ty(Ctx)) {}

  bool run(Function &F);

private:
  static bool needsExpansion(Constant *C);
  void expandConstants(Instruction *I);

  std::pair<Value *, Value *> split(IRBuilder<> &B, Value *P) {
    Value *Raw = B.CreatePtrToInt(P, I32);
    Value *Off = B.CreateTrunc(Raw, I16);
    Value *Seg = B.CreateTrunc(B.CreateLShr(Raw, 16), I16);
    return {Seg, Off};
  }
  Value *make(IRBuilder<> &B, Value *Seg, Value *Off, Type *PtrTy) {
    Value *Raw = B.CreateOr(B.CreateShl(B.CreateZExt(Seg, I32), 16),
                            B.CreateZExt(Off, I32));
    return B.CreateIntToPtr(Raw, PtrTy);
  }
  Value *linear(IRBuilder<> &B, Value *P) {
    auto [Seg, Off] = split(B, P);
    return B.CreateAdd(B.CreateShl(B.CreateZExt(Seg, I32), 4),
                       B.CreateZExt(Off, I32));
  }
  Value *fromLinear(IRBuilder<> &B, Value *L, Type *PtrTy) {
    Value *Seg = B.CreateTrunc(B.CreateLShr(L, 4), I16);
    Value *Off = B.CreateTrunc(B.CreateAnd(L, 15), I16);
    return make(B, Seg, Off, PtrTy);
  }

  Value *lowerGEP(GetElementPtrInst *GEP);
};
} // namespace

bool HugePointers::needsExpansion(Constant *C) {
  auto *CE = dyn_cast<ConstantExpr>(C);
  if (!CE)
    return false;
  if (isHugePtr(CE->getType()) && CE->getOpcode() != Instruction::AddrSpaceCast)
    return true;
  if ((CE->getOpcode() == Instruction::PtrToInt ||
       CE->getOpcode() == Instruction::PtrToAddr) &&
      isHugePtr(CE->getOperand(0)->getType()))
    return true;
  for (Value *Op : CE->operands())
    if (auto *OpC = dyn_cast<Constant>(Op))
      if (needsExpansion(OpC))
        return true;
  return false;
}

// Turn the constant expressions that compute with huge pointers into
// instructions, so that they are lowered like the others.
void HugePointers::expandConstants(Instruction *I) {
  for (unsigned N = 0, E = I->getNumOperands(); N != E; ++N) {
    auto *C = dyn_cast<Constant>(I->getOperand(N));
    if (!C || !needsExpansion(C))
      continue;
    Instruction *At = I;
    if (auto *PN = dyn_cast<PHINode>(I))
      At = PN->getIncomingBlock(N)->getTerminator();
    Instruction *New = cast<ConstantExpr>(C)->getAsInstruction();
    New->insertBefore(At->getIterator());
    expandConstants(New);
    I->setOperand(N, New);
  }
}

Value *HugePointers::lowerGEP(GetElementPtrInst *GEP) {
  IRBuilder<> B(GEP);
  Value *Ptr = GEP->getPointerOperand();
  Value *Bytes = emitGEPOffset(&B, DL, GEP, /*NoAssumptions=*/true);
  Bytes = B.CreateSExtOrTrunc(Bytes, I32);
  auto [Seg, Off] = split(B, Ptr);

  // A small constant offset used only to load and store does not need to
  // carry into the segment: a normalized offset has room for it, and an
  // unnormalized pointer points into an object within its segment.
  if (auto *CI = dyn_cast<ConstantInt>(Bytes)) {
    int64_t C = CI->getSExtValue();
    bool OnlyAccessed = all_of(GEP->users(), [&](User *U) {
      if (auto *L = dyn_cast<LoadInst>(U))
        return L->getPointerOperand() == GEP;
      if (auto *S = dyn_cast<StoreInst>(U))
        return S->getPointerOperand() == GEP && S->getValueOperand() != GEP;
      return false;
    });
    if (C >= 0 && C < 0x8000 && OnlyAccessed)
      return make(B, Seg, B.CreateAdd(Off, ConstantInt::get(I16, C)),
                  GEP->getType());
  }

  Value *T = B.CreateAdd(B.CreateZExt(Off, I32), Bytes);
  Value *NewSeg = B.CreateAdd(Seg, B.CreateTrunc(B.CreateAShr(T, 4), I16));
  Value *NewOff = B.CreateTrunc(B.CreateAnd(T, 15), I16);
  return make(B, NewSeg, NewOff, GEP->getType());
}

bool HugePointers::run(Function &F) {
  SmallVector<Instruction *, 32> Work;
  for (Instruction &I : instructions(F)) {
    expandConstants(&I);
  }
  for (Instruction &I : instructions(F)) {
    if (auto *GEP = dyn_cast<GetElementPtrInst>(&I)) {
      if (isHugePtr(GEP->getType()))
        Work.push_back(GEP);
    } else if (isa<PtrToIntInst, PtrToAddrInst>(&I)) {
      if (isHugePtr(I.getOperand(0)->getType()))
        Work.push_back(&I);
    } else if (auto *I2P = dyn_cast<IntToPtrInst>(&I)) {
      if (isHugePtr(I2P->getType()))
        Work.push_back(I2P);
    } else if (auto *Cmp = dyn_cast<ICmpInst>(&I)) {
      if (isHugePtr(Cmp->getOperand(0)->getType()))
        Work.push_back(Cmp);
    }
  }

  for (Instruction *I : Work) {
    IRBuilder<> B(I);
    Value *New = nullptr;
    if (auto *GEP = dyn_cast<GetElementPtrInst>(I)) {
      New = lowerGEP(GEP);
    } else if (isa<PtrToIntInst, PtrToAddrInst>(I)) {
      New = B.CreateZExtOrTrunc(linear(B, I->getOperand(0)), I->getType());
    } else if (isa<IntToPtrInst>(I)) {
      New = fromLinear(B, B.CreateZExtOrTrunc(I->getOperand(0), I32),
                       I->getType());
    } else {
      auto *Cmp = cast<ICmpInst>(I);
      New = B.CreateICmp(Cmp->getPredicate(), linear(B, Cmp->getOperand(0)),
                         linear(B, Cmp->getOperand(1)));
    }
    New->takeName(I);
    I->replaceAllUsesWith(New);
  }
  for (Instruction *I : Work)
    I->eraseFromParent();
  return !Work.empty();
}

namespace {
class X86HugePointersLegacy : public FunctionPass {
public:
  static char ID;
  X86HugePointersLegacy() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    if (!F.getParent()->getTargetTriple().isX86_16())
      return false;
    return HugePointers(F).run(F);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }

  StringRef getPassName() const override { return "X86 i8086 Huge Pointers"; }
};
} // namespace

char X86HugePointersLegacy::ID = 0;

INITIALIZE_PASS(X86HugePointersLegacy, DEBUG_TYPE, "X86 i8086 Huge Pointers",
                false, false)

FunctionPass *llvm::createX86HugePointersLegacyPass() {
  return new X86HugePointersLegacy();
}

PreservedAnalyses X86HugePointersPass::run(Function &F,
                                           FunctionAnalysisManager &) {
  if (!F.getParent()->getTargetTriple().isX86_16() ||
      !HugePointers(F).run(F))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}
