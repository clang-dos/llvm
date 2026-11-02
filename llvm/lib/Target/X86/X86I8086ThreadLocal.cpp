//===-- X86I8086ThreadLocal.cpp - Thread-local variables on i8086 ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A DOS or 16-bit Windows program has one thread, and there is no TLS: no
// thread pointer, no TLS segment and no OMF record for one. A thread-local
// variable (C11 _Thread_local, C++ thread_local) is then an ordinary
// variable. This pass makes it one before instruction selection, which would
// otherwise use the Windows TLS sequence through FS (an 80386 register) and
// put the variable in a .tbss or .tdata section.
//
// Calls to llvm.threadlocal.address are replaced by their operand, since the
// intrinsic needs a thread-local global.
//
//===----------------------------------------------------------------------===//

#include "X86.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

#define DEBUG_TYPE "x86-i8086-thread-local"

namespace {

class X86I8086ThreadLocal : public ModulePass {
public:
  static char ID;
  X86I8086ThreadLocal() : ModulePass(ID) {}
  StringRef getPassName() const override {
    return "X86 i8086 thread-local variables";
  }
  bool runOnModule(Module &M) override;
};

} // end anonymous namespace

char X86I8086ThreadLocal::ID = 0;

INITIALIZE_PASS(X86I8086ThreadLocal, DEBUG_TYPE,
                "X86 i8086 thread-local variables", false, false)

ModulePass *llvm::createX86I8086ThreadLocalPass() {
  return new X86I8086ThreadLocal();
}

bool X86I8086ThreadLocal::runOnModule(Module &M) {
  if (Triple(M.getTargetTriple()).getArch() != Triple::i8086)
    return false;
  bool Changed = false;

  // First the uses of llvm.threadlocal.address, while the verifier would
  // still accept them.
  for (Function &F : M) {
    if (F.getIntrinsicID() != Intrinsic::threadlocal_address)
      continue;
    for (User *U : make_early_inc_range(F.users())) {
      auto *II = dyn_cast<IntrinsicInst>(U);
      if (!II || II->getCalledFunction() != &F)
        continue;
      II->replaceAllUsesWith(II->getArgOperand(0));
      II->eraseFromParent();
      Changed = true;
    }
  }

  for (GlobalVariable &GV : M.globals())
    if (GV.isThreadLocal()) {
      GV.setThreadLocal(false);
      Changed = true;
    }
  for (GlobalAlias &GA : M.aliases())
    if (GA.isThreadLocal()) {
      GA.setThreadLocal(false);
      Changed = true;
    }
  return Changed;
}
