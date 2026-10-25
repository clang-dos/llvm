//===-- X86I8086InterruptArgs.cpp - SS-relative interrupt registers -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// An i8086 interrupt function (x86_intrcc) runs on the stack of the program
// it interrupted, so SS is not DS. Its pointer parameter points at the
// registers its prologue saved on that stack (union INTPACK). clang makes
// it a near stack pointer (address space 6), whose cast to a far pointer
// takes SS, but a near data pointer (address space 0) means DS: an access
// through it that the code generator does not fold into a BP-relative
// address, such as one in another basic block, would read DGROUP instead.
// This pass makes the loads and stores that the pointer reaches through
// GEPs and casts to near pointers SS-relative (address space 258), which
// gives them an SS: prefix. Other uses (a call argument, a stored pointer)
// keep the pointer they had.
//
//===----------------------------------------------------------------------===//

#include "X86.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

#define DEBUG_TYPE "x86-i8086-interrupt-args"

namespace {

class X86I8086InterruptArgsLegacy : public FunctionPass {
public:
  static char ID;
  X86I8086InterruptArgsLegacy() : FunctionPass(ID) {}

  StringRef getPassName() const override {
    return "X86 i8086 interrupt function arguments";
  }

  bool runOnFunction(Function &F) override;
};

} // end anonymous namespace

char X86I8086InterruptArgsLegacy::ID = 0;

INITIALIZE_PASS(X86I8086InterruptArgsLegacy, DEBUG_TYPE,
                "X86 i8086 interrupt function arguments", false, false)

FunctionPass *llvm::createX86I8086InterruptArgsLegacyPass() {
  return new X86I8086InterruptArgsLegacy();
}

/// Rewrite the loads, stores and GEPs that use Old (a near pointer) to use
/// New (the same pointer in address space SS). Other uses get New cast back.
static bool rewriteUses(Value *Old, Value *New) {
  bool Changed = false;
  for (Use &U : make_early_inc_range(Old->uses())) {
    auto *I = dyn_cast<Instruction>(U.getUser());
    if (!I)
      continue;
    if (auto *LI = dyn_cast<LoadInst>(I)) {
      LI->setOperand(LI->getPointerOperandIndex(), New);
      Changed = true;
      continue;
    }
    if (auto *SI = dyn_cast<StoreInst>(I);
        SI && U.getOperandNo() == SI->getPointerOperandIndex()) {
      SI->setOperand(SI->getPointerOperandIndex(), New);
      Changed = true;
      continue;
    }
    if (auto *ASC = dyn_cast<AddrSpaceCastInst>(I); ASC && ASC != New) {
      unsigned AS = ASC->getDestAddressSpace();
      if (AS == 0) {
        Changed |= rewriteUses(ASC, New);
        if (ASC->use_empty())
          ASC->eraseFromParent();
        continue;
      }
      // A far pointer to the registers: cast from a near stack pointer,
      // which takes SS as its segment.
      if (AS == X86AS::FAR_DATA || AS == X86AS::HUGE_DATA) {
        IRBuilder<> B(ASC);
        U.set(B.CreateAddrSpaceCast(
            New, PointerType::get(I->getContext(), X86AS::NEAR_STACK)));
        Changed = true;
        continue;
      }
    }
    if (auto *GEP = dyn_cast<GetElementPtrInst>(I);
        GEP && U.getOperandNo() == GEP->getPointerOperandIndex()) {
      IRBuilder<> B(GEP);
      SmallVector<Value *, 4> Indices(GEP->indices());
      Value *NewGEP = B.CreateGEP(GEP->getSourceElementType(), New, Indices,
                                  GEP->getName(), GEP->getNoWrapFlags());
      rewriteUses(GEP, NewGEP);
      if (GEP->use_empty())
        GEP->eraseFromParent();
      Changed = true;
      continue;
    }
    // Anything else keeps the pointer it had.
    if (I == New || !Old->getType()->isPointerTy() ||
        Old->getType()->getPointerAddressSpace() != 0)
      continue;
    IRBuilder<> B(I);
    if (auto *PN = dyn_cast<PHINode>(I))
      B.SetInsertPoint(PN->getIncomingBlock(U)->getTerminator());
    U.set(B.CreateAddrSpaceCast(New, Old->getType()));
    Changed = true;
  }
  return Changed;
}

bool X86I8086InterruptArgsLegacy::runOnFunction(Function &F) {
  if (F.getCallingConv() != CallingConv::X86_INTR ||
      F.getParent()->getTargetTriple().getArch() != Triple::i8086)
    return false;
  bool Changed = false;
  for (Argument &A : F.args()) {
    auto *PT = dyn_cast<PointerType>(A.getType());
    if (!PT || (PT->getAddressSpace() != 0 && PT->getAddressSpace() != 6) ||
        A.use_empty())
      continue;
    IRBuilder<> B(&*F.getEntryBlock().getFirstInsertionPt());
    Value *SS = B.CreateAddrSpaceCast(
        &A, PointerType::get(F.getContext(), X86AS::SS), A.getName() + ".ss");
    Changed |= rewriteUses(&A, SS);
  }
  return Changed;
}
