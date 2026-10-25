//===-- X86FixupAddr16.cpp - Legalize 8086 memory operands ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The 8086 can only address memory through BX, BP, SI or DI alone, or through
// one of BX/BP plus one of SI/DI, with no scale. Instruction selection uses
// plain GR16 operands, so before register allocation this pass copies the
// base and index registers of every memory operand into virtual registers of
// classes that only hold those registers (also when the same value is used as
// both base and index, or when the base is SP).
//
// BP is the frame pointer, so it only appears through frame indices. A base
// register alone goes in BX or SI, and a base plus an index in BX plus SI or
// DI. These register sets have 32-bit and 64-bit counterparts that
// already exist, so the classes do not change register pressure tracking for
// other x86 targets.
//
// On 16-bit Windows, a DLL runs on its caller's stack, so SS is not DS. An
// access to a local variable through BX, SI or DI (which use DS) gets an SS
// segment override. The same holds in an interrupt function, which runs on
// the stack of the program it interrupted. Accesses through BP already use
// SS, so an SS override on a frame index is removed.
//
// After register allocation, a second pass clears the renamable flag on these
// registers, so that later passes such as MachineCopyPropagation, which only
// know the plain GR16 operand classes, do not replace them.
//
//===----------------------------------------------------------------------===//

#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/ValueTracking.h"

using namespace llvm;

#define DEBUG_TYPE "x86-fixup-addr16"

namespace {

class X86FixupAddr16Legacy : public MachineFunctionPass {
public:
  static char ID;
  X86FixupAddr16Legacy() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "X86 16-bit Addressing Fixup";
  }

private:
  bool fixupInstr(MachineInstr &MI, unsigned MemOp);
  bool addStackSegment(MachineInstr &MI, unsigned MemOp);
  bool copyIfNeeded(MachineInstr &MI, MachineOperand &MO,
                    const TargetRegisterClass *RC);

  const X86InstrInfo *TII = nullptr;
  MachineRegisterInfo *MRI = nullptr;
};

class X86ProtectAddr16Legacy : public MachineFunctionPass {
public:
  static char ID;
  X86ProtectAddr16Legacy() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override {
    return "X86 16-bit Addressing Register Protection";
  }
};

/// Returns the index of the memory operand of \p MI, or -1.
int getMemOperandIdx(const MachineInstr &MI) {
  const MCInstrDesc &Desc = MI.getDesc();
  int MemOp = X86II::getMemoryOperandNo(Desc.TSFlags);
  if (MemOp < 0)
    return -1;
  MemOp += X86II::getOperandBias(Desc);
  if (MI.getNumOperands() < unsigned(MemOp) + X86::AddrNumOperands)
    return -1;
  return MemOp;
}

} // end anonymous namespace

char X86ProtectAddr16Legacy::ID = 0;

INITIALIZE_PASS(X86ProtectAddr16Legacy, "x86-protect-addr16",
                "X86 16-bit Addressing Register Protection", false, false)

FunctionPass *llvm::createX86ProtectAddr16LegacyPass() {
  return new X86ProtectAddr16Legacy();
}

/// Removes jumps to the next block, which -O0 leaves after every block, as
/// no branch folding or block placement runs. Code is limited to 64 KiB.
static bool removeFallthroughJumps(MachineFunction &MF) {
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    MachineBasicBlock *TBB = nullptr, *FBB = nullptr;
    SmallVector<MachineOperand, 4> Cond;
    if (TII->analyzeBranch(MBB, TBB, FBB, Cond) || !TBB)
      continue;
    if (Cond.empty()) {
      // An unconditional jump.
      if (!MBB.isLayoutSuccessor(TBB))
        continue;
      TII->removeBranch(MBB);
    } else if (FBB && MBB.isLayoutSuccessor(FBB)) {
      // A conditional jump and a jump to the next block.
      TII->removeBranch(MBB);
      TII->insertBranch(MBB, TBB, nullptr, Cond, DebugLoc());
    } else if (FBB && MBB.isLayoutSuccessor(TBB) &&
               !TII->reverseBranchCondition(Cond)) {
      // A conditional jump to the next block and a jump elsewhere.
      TII->removeBranch(MBB);
      TII->insertBranch(MBB, FBB, nullptr, Cond, DebugLoc());
    } else {
      continue;
    }
    Changed = true;
  }
  return Changed;
}

namespace {
class X86RemoveFallthroughJumpsLegacy : public MachineFunctionPass {
public:
  static char ID;
  X86RemoveFallthroughJumpsLegacy() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    if (!MF.getSubtarget<X86Subtarget>().isTarget16BitABI())
      return false;
    return removeFallthroughJumps(MF);
  }

  StringRef getPassName() const override {
    return "X86 16-bit Fallthrough Jump Removal";
  }
};
} // end anonymous namespace

char X86RemoveFallthroughJumpsLegacy::ID = 0;

INITIALIZE_PASS(X86RemoveFallthroughJumpsLegacy, "x86-remove-fallthrough-jumps",
                "X86 16-bit Fallthrough Jump Removal", false, false)

FunctionPass *llvm::createX86RemoveFallthroughJumpsLegacyPass() {
  return new X86RemoveFallthroughJumpsLegacy();
}

bool X86ProtectAddr16Legacy::runOnMachineFunction(MachineFunction &MF) {
  if (!MF.getSubtarget<X86Subtarget>().isTarget16BitABI())
    return false;
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      int MemOp = getMemOperandIdx(MI);
      if (MemOp < 0)
        continue;
      for (unsigned Op : {X86::AddrBaseReg, X86::AddrIndexReg}) {
        MachineOperand &MO = MI.getOperand(MemOp + Op);
        if (MO.isReg() && MO.getReg() && MO.isRenamable()) {
          MO.setIsRenamable(false);
          Changed = true;
        }
      }
    }
  }
  return Changed;
}

char X86FixupAddr16Legacy::ID = 0;

INITIALIZE_PASS(X86FixupAddr16Legacy, DEBUG_TYPE, "X86 16-bit Addressing Fixup",
                false, false)

FunctionPass *llvm::createX86FixupAddr16LegacyPass() {
  return new X86FixupAddr16Legacy();
}

// Make the register in MO a member of RC. Unless it already is one, the value
// is copied into a new virtual register just before MI. Constraining the
// register itself instead would tie a long live range to one or two registers,
// which can leave the allocator without registers; the register coalescer
// removes the copy when that is safe (see X86RegisterInfo::shouldCoalesce).
bool X86FixupAddr16Legacy::copyIfNeeded(MachineInstr &MI, MachineOperand &MO,
                                        const TargetRegisterClass *RC) {
  Register Reg = MO.getReg();
  if (Reg.isVirtual() && !MO.getSubReg() &&
      RC->hasSubClassEq(MRI->getRegClass(Reg)))
    return false;
  if (Reg.isPhysical() && RC->contains(Reg))
    return false;

  Register NewReg = MRI->createVirtualRegister(RC);
  BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(TargetOpcode::COPY),
          NewReg)
      .addReg(Reg, RegState::NoFlags, MO.getSubReg());
  MO.setReg(NewReg);
  MO.setSubReg(0);
  MO.setIsKill(true);
  return true;
}

bool X86FixupAddr16Legacy::fixupInstr(MachineInstr &MI, unsigned MemOp) {
  MachineOperand &Base = MI.getOperand(MemOp + X86::AddrBaseReg);
  MachineOperand &Scale = MI.getOperand(MemOp + X86::AddrScaleAmt);
  MachineOperand &Index = MI.getOperand(MemOp + X86::AddrIndexReg);

  bool HasBase = !Base.isReg() || Base.getReg();
  bool HasIndex = Index.isReg() && Index.getReg();
  bool Changed = false;

  if (HasIndex && Scale.isImm() && Scale.getImm() != 1)
    report_fatal_error("scaled index in an i8086 memory operand");

  // An index alone is the same as a base alone, which allows BX and BP too.
  if (HasIndex && !HasBase) {
    Base.setReg(Index.getReg());
    Base.setSubReg(Index.getSubReg());
    Base.setIsKill(Index.isKill());
    Index.setReg(Register());
    Index.setSubReg(0);
    Index.setIsKill(false);
    HasBase = true;
    HasIndex = false;
    Changed = true;
  }

  // A frame index becomes BP.
  bool BaseIsReg = Base.isReg() && Base.getReg();

  if (!HasIndex) {
    if (BaseIsReg)
      Changed |= copyIfNeeded(MI, Base, &X86::I8086_BXSIRegClass);
    return Changed;
  }

  // BP+SI, BP+DI, BX+SI or BX+DI. If the base and the index are the same
  // register, the index has to be copied.
  if (BaseIsReg) {
    Changed |= copyIfNeeded(MI, Base, &X86::I8086_BXRegClass);
    if (Index.getReg() == Base.getReg()) {
      Register NewReg = MRI->createVirtualRegister(&X86::I8086_SIDIRegClass);
      BuildMI(*MI.getParent(), MI, MI.getDebugLoc(),
              TII->get(TargetOpcode::COPY), NewReg)
          .addReg(Index.getReg(), RegState::NoFlags, Index.getSubReg());
      Index.setReg(NewReg);
      Index.setSubReg(0);
      Index.setIsKill(true);
      Base.setIsKill(false);
      return true;
    }
  }
  Changed |= copyIfNeeded(MI, Index, &X86::I8086_SIDIRegClass);
  return Changed;
}

/// Returns true if \p MMO accesses the stack in the default address space.
static bool isStackAccess(const MachineMemOperand *MMO) {
  if (const PseudoSourceValue *PSV = MMO->getPseudoValue())
    return PSV->isStack() || PSV->kind() == PseudoSourceValue::FixedStack;
  const Value *V = MMO->getValue();
  if (!V || MMO->getAddrSpace() != 0)
    return false;
  SmallVector<const Value *, 4> Objs;
  getUnderlyingObjects(V, Objs);
  if (Objs.empty())
    return false;
  for (const Value *O : Objs) {
    if (isa<AllocaInst>(O))
      continue;
    if (const auto *A = dyn_cast<Argument>(O); A && A->hasByValAttr())
      continue;
    return false;
  }
  return true;
}

/// Returns true if \p R is SS, or a copy of it.
static bool isSS(Register R, const MachineRegisterInfo &MRI) {
  while (R.isVirtual()) {
    const MachineInstr *Def = MRI.getVRegDef(R);
    if (!Def || !Def->isCopy() || Def->getOperand(1).getSubReg())
      return false;
    R = Def->getOperand(1).getReg();
  }
  return R == X86::SS;
}

/// Removes an SS override from a frame index, which becomes BP and uses SS
/// anyway.
static bool removeStackSegment(MachineInstr &MI, unsigned MemOp,
                               const MachineRegisterInfo &MRI) {
  MachineOperand &Base = MI.getOperand(MemOp + X86::AddrBaseReg);
  MachineOperand &Seg = MI.getOperand(MemOp + X86::AddrSegmentReg);
  if (!Base.isFI() || !Seg.isReg() || !Seg.getReg() || !isSS(Seg.getReg(), MRI))
    return false;
  Seg.setReg(Register());
  return true;
}

/// Adds an SS override to a stack access through a register other than BP.
bool X86FixupAddr16Legacy::addStackSegment(MachineInstr &MI, unsigned MemOp) {
  MachineOperand &Base = MI.getOperand(MemOp + X86::AddrBaseReg);
  MachineOperand &Seg = MI.getOperand(MemOp + X86::AddrSegmentReg);
  if (!Base.isReg() || !Base.getReg() || !Seg.isReg() || Seg.getReg())
    return false;
  if (MI.getOpcode() == X86::LEA16r || MI.memoperands_empty())
    return false;
  if (!llvm::all_of(MI.memoperands(), isStackAccess))
    return false;
  Seg.setReg(X86::SS);
  return true;
}

bool X86FixupAddr16Legacy::runOnMachineFunction(MachineFunction &MF) {
  const X86Subtarget &ST = MF.getSubtarget<X86Subtarget>();
  if (!ST.isTarget16BitABI())
    return false;

  TII = ST.getInstrInfo();
  MRI = &MF.getRegInfo();
  bool Changed = false;
  // An interrupt function runs on the stack of the program it interrupted.
  bool SeparateStack =
      ST.getTargetTriple().isOSWin16() ||
      MF.getFunction().getCallingConv() == CallingConv::X86_INTR;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      // A 32-bit LEA (80386 and later) uses 32-bit addressing.
      if (MI.getOpcode() == X86::LEA32r || MI.getOpcode() == X86::LEA64_32r)
        continue;
      int MemOp = getMemOperandIdx(MI);
      if (MemOp < 0)
        continue;
      Changed |= fixupInstr(MI, MemOp);
      Changed |= removeStackSegment(MI, MemOp, *MRI);
      if (SeparateStack)
        Changed |= addStackSegment(MI, MemOp);
    }
  }

  // A far pointer to a local copies SS into ES, which is left unused once
  // removeStackSegment has its loads and stores go through BP. Without
  // optimization nothing else removes these copies.
  bool Removed;
  do {
    Removed = false;
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : llvm::make_early_inc_range(llvm::reverse(MBB)))
        if (MI.isCopy() && MI.getOperand(0).getReg().isVirtual() &&
            MRI->use_nodbg_empty(MI.getOperand(0).getReg())) {
          Register Src = MI.getOperand(1).getReg();
          if (Src.isPhysical() && !X86::SEGMENT_REGRegClass.contains(Src))
            continue;
          for (MachineOperand &MO : llvm::make_early_inc_range(
                   MRI->reg_operands(MI.getOperand(0).getReg())))
            if (MO.isDebug())
              MO.setReg(Register());
          MI.eraseFromParent();
          Removed = Changed = true;
        }
  } while (Removed);
  return Changed;
}
