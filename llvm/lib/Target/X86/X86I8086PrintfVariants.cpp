//===-- X86I8086PrintfVariants.cpp - Smaller printf and scanf on i8086 ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// picolibc builds printf and scanf in variants: integer only ("i"), integers
// with long long ("l"), and the full one with floating point ("d"), which is
// what printf and scanf are. On the 8086 the full printf is about 13 KB of
// code with its floating-point conversion and 64-bit division, against about
// 2 KB for the integer one, so a program that only prints integers and strings
// should not link it.
//
// This pass looks at each call to printf, fprintf, sprintf, snprintf,
// asprintf, scanf, fscanf and sscanf whose format is a constant
// string, and calls __i_<name> or __l_<name> instead when every conversion in
// the format is one that variant handles: no floating point, positional
// arguments or wide characters, and long long only in the "l" variant. A
// call with any other format, or a floating-point argument, is left alone.
// The v* functions take a va_list whose contents the call does not show, so
// they always use the full variant.
//
//===----------------------------------------------------------------------===//

#include "X86.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

#define DEBUG_TYPE "x86-i8086-printf-variants"

namespace {

enum Variant { Integer, LongLong, Full };

// The variant a printf format needs.
Variant printfVariant(StringRef Fmt) {
  Variant V = Integer;
  for (size_t I = 0, E = Fmt.size(); I < E; ++I) {
    if (Fmt[I] != '%')
      continue;
    if (++I == E)
      return Full;
    if (Fmt[I] == '%')
      continue;
    while (I < E && StringRef("-+ #0").contains(Fmt[I]))
      ++I;
    while (I < E && (isDigit(Fmt[I]) || Fmt[I] == '*' || Fmt[I] == '.'))
      ++I;
    if (I < E && Fmt[I] == '$') // Positional arguments.
      return Full;
    bool Long = false;
    if (I + 1 < E && ((Fmt[I] == 'h' && Fmt[I + 1] == 'h') ||
                      (Fmt[I] == 'l' && Fmt[I + 1] == 'l'))) {
      Long = Fmt[I] == 'l';
      I += 2;
    } else if (I < E && StringRef("hlzt").contains(Fmt[I])) {
      if (Fmt[I] == 'l' && I + 1 < E && StringRef("cs").contains(Fmt[I + 1]))
        return Full; // Wide characters.
      ++I;
    } else if (I < E && (Fmt[I] == 'j' || Fmt[I] == 'q')) {
      Long = true;
      ++I;
    }
    if (I == E || !StringRef("diouxXcspbB").contains(Fmt[I]))
      return Full;
    if (Long)
      V = LongLong;
  }
  return V;
}

// The variant a scanf format needs.
Variant scanfVariant(StringRef Fmt) {
  Variant V = Integer;
  for (size_t I = 0, E = Fmt.size(); I < E; ++I) {
    if (Fmt[I] != '%')
      continue;
    if (++I == E)
      return Full;
    if (Fmt[I] == '%')
      continue;
    if (Fmt[I] == '*')
      ++I;
    while (I < E && isDigit(Fmt[I]))
      ++I;
    if (I < E && Fmt[I] == '$')
      return Full;
    bool Long = false;
    if (I + 1 < E && ((Fmt[I] == 'h' && Fmt[I + 1] == 'h') ||
                      (Fmt[I] == 'l' && Fmt[I + 1] == 'l'))) {
      Long = Fmt[I] == 'l';
      I += 2;
    } else if (I < E && StringRef("hlzt").contains(Fmt[I])) {
      if (Fmt[I] == 'l' && I + 1 < E && StringRef("cs[").contains(Fmt[I + 1]))
        return Full; // Wide characters.
      ++I;
    } else if (I < E && (Fmt[I] == 'j' || Fmt[I] == 'q')) {
      Long = true;
      ++I;
    }
    if (I == E || !StringRef("diouxXcsp[").contains(Fmt[I]))
      return Full;
    if (Fmt[I] == '[') {
      // A set: ']' first in it is a member, not its end.
      ++I;
      if (I < E && Fmt[I] == '^')
        ++I;
      if (I < E && Fmt[I] == ']')
        ++I;
      while (I < E && Fmt[I] != ']')
        ++I;
      if (I == E)
        return Full;
    }
    if (Long)
      V = LongLong;
  }
  return V;
}

class X86I8086PrintfVariants : public ModulePass {
public:
  static char ID;
  X86I8086PrintfVariants() : ModulePass(ID) {}
  StringRef getPassName() const override {
    return "X86 i8086 printf and scanf variants";
  }
  bool runOnModule(Module &M) override;
};

} // end anonymous namespace

char X86I8086PrintfVariants::ID = 0;

INITIALIZE_PASS(X86I8086PrintfVariants, DEBUG_TYPE,
                "X86 i8086 printf and scanf variants", false, false)

ModulePass *llvm::createX86I8086PrintfVariantsPass() {
  return new X86I8086PrintfVariants();
}

bool X86I8086PrintfVariants::runOnModule(Module &M) {
  if (Triple(M.getTargetTriple()).getArch() != Triple::i8086)
    return false;
  bool Changed = false;
  // Copy the list: the variants are added to the module as we go.
  SmallVector<Function *, 8> Funcs;
  for (Function &F : M)
    if (F.isDeclaration() && F.isVarArg())
      Funcs.push_back(&F);
  for (Function *F : Funcs) {
    // The format's argument number, and whether the function is a scanf.
    int FmtArg;
    bool Scanf;
    std::tie(FmtArg, Scanf) = StringSwitch<std::pair<int, bool>>(F->getName())
                                  .Case("printf", {0, false})
                                  .Case("fprintf", {1, false})
                                  .Case("sprintf", {1, false})
                                  .Case("snprintf", {2, false})
                                  .Case("asprintf", {1, false})
                                  .Case("scanf", {0, true})
                                  .Case("fscanf", {1, true})
                                  .Case("sscanf", {1, true})
                                  .Default({-1, false});
    if (FmtArg < 0 || F->arg_size() != unsigned(FmtArg) + 1)
      continue;
    for (User *U : make_early_inc_range(F->users())) {
      auto *CB = dyn_cast<CallBase>(U);
      if (!CB || CB->getCalledOperand() != F)
        continue;
      StringRef Fmt;
      if (!getConstantStringInfo(
              CB->getArgOperand(FmtArg)->stripPointerCasts(), Fmt))
        continue;
      Variant V = Scanf ? scanfVariant(Fmt) : printfVariant(Fmt);
      for (unsigned I = FmtArg + 1, E = CB->arg_size(); I < E; ++I) {
        Type *T = CB->getArgOperand(I)->getType();
        if (T->isFloatingPointTy())
          V = Full;
        else if (T->isIntegerTy() && T->getIntegerBitWidth() > 32 && V != Full)
          V = LongLong;
      }
      if (V == Full)
        continue;
      std::string Name = (V == Integer ? "__i_" : "__l_") + F->getName().str();
      Function *NewF = M.getFunction(Name);
      if (!NewF) {
        NewF = Function::Create(F->getFunctionType(), F->getLinkage(),
                                F->getAddressSpace(), Name, &M);
        NewF->copyAttributesFrom(F);
      }
      if (NewF->getFunctionType() != F->getFunctionType())
        continue;
      CB->setCalledOperand(NewF);
      Changed = true;
    }
  }
  return Changed;
}
