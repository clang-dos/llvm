//===---- TargetInfo.cpp - Encapsulate target details -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// These classes wrap the information about a call or function
// definition used to handle ABI compliancy.
//
//===----------------------------------------------------------------------===//

#include "TargetInfo.h"
#include "ABIInfo.h"
#include "ABIInfoImpl.h"
#include "CodeGenFunction.h"
#include "clang/Basic/CodeGenOptions.h"
#include "clang/CodeGen/CGFunctionInfo.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace CodeGen;

LLVM_DUMP_METHOD void ABIArgInfo::dump() const {
  raw_ostream &OS = llvm::errs();
  OS << "(ABIArgInfo Kind=";
  switch (TheKind) {
  case Direct:
    OS << "Direct Type=";
    if (llvm::Type *Ty = getCoerceToType())
      Ty->print(OS);
    else
      OS << "null";
    break;
  case Extend:
    OS << "Extend";
    break;
  case Ignore:
    OS << "Ignore";
    break;
  case InAlloca:
    OS << "InAlloca Offset=" << getInAllocaFieldIndex();
    break;
  case Indirect:
    OS << "Indirect Align=" << getIndirectAlign().getQuantity()
       << " ByVal=" << getIndirectByVal()
       << " Realign=" << getIndirectRealign();
    break;
  case IndirectAliased:
    OS << "Indirect Align=" << getIndirectAlign().getQuantity()
       << " AadrSpace=" << getIndirectAddrSpace()
       << " Realign=" << getIndirectRealign();
    break;
  case Expand:
    OS << "Expand";
    break;
  case CoerceAndExpand:
    OS << "CoerceAndExpand Type=";
    getCoerceAndExpandType()->print(OS);
    break;
  case TargetSpecific:
    OS << "TargetSpecific Type=";
    if (llvm::Type *Ty = getCoerceToType())
      Ty->print(OS);
    else
      OS << "null";
    break;
  }
  OS << ")\n";
}

TargetCodeGenInfo::TargetCodeGenInfo(std::unique_ptr<ABIInfo> Info)
    : Info(std::move(Info)) {}

TargetCodeGenInfo::~TargetCodeGenInfo() = default;

// If someone can figure out a general rule for this, that would be great.
// It's probably just doomed to be platform-dependent, though.
unsigned TargetCodeGenInfo::getSizeOfUnwindException() const {
  if (getABIInfo().getCodeGenOpts().hasSEHExceptions())
    return getABIInfo().getDataLayout().getPointerSizeInBits() > 32 ? 64 : 48;
  // Verified for:
  //   x86-64     FreeBSD, Linux, Darwin
  //   x86-32     FreeBSD, Linux, Darwin
  //   PowerPC    Linux
  //   ARM        Darwin (*not* EABI)
  //   AArch64    Linux
  return 32;
}

bool TargetCodeGenInfo::isNoProtoCallVariadic(const CallArgList &args,
                                     const FunctionNoProtoType *fnType) const {
  // The following conventions are known to require this to be false:
  //   x86_stdcall
  //   MIPS
  // For everything else, we just prefer false unless we opt out.
  return false;
}

void
TargetCodeGenInfo::getDependentLibraryOption(llvm::StringRef Lib,
                                             llvm::SmallString<24> &Opt) const {
  // This assumes the user is passing a library name like "rt" instead of a
  // filename like "librt.a/so", and that they don't care whether it's static or
  // dynamic.
  Opt = "-l";
  Opt += Lib;
}

unsigned TargetCodeGenInfo::getDeviceKernelCallingConv() const {
  // Device kernels are called via an explicit runtime API with arguments,
  // such as set with clSetKernelArg() for OpenCL, not as normal
  // sub-functions.  This uses a modified version of the C calling convention
  // which simplifies the treatment of non-scalar types.  (The rule adjustment
  // hapens in CodeGenTypes::arrangeLLVMFunctionInfo.)
  //
  // Outside of OpenCL, kernels currently do not exist for CPU targets.
  assert(getABIInfo().getContext().getLangOpts().OpenCL &&
         "Kernel calling convention only defined for OpenCL");
  return llvm::CallingConv::C;
}

void TargetCodeGenInfo::setOCLKernelStubCallingConvention(
    const FunctionType *&FT) const {
  FT = getABIInfo().getContext().adjustFunctionType(
      FT, FT->getExtInfo().withCallingConv(CC_C));
}

llvm::Constant *TargetCodeGenInfo::getNullPointer(const CodeGen::CodeGenModule &CGM,
    llvm::PointerType *T, QualType QT) const {
  return llvm::ConstantPointerNull::get(T);
}

LangAS TargetCodeGenInfo::getGlobalVarAddressSpace(CodeGenModule &CGM,
                                                   const VarDecl *D) const {
  assert(!CGM.getLangOpts().OpenCL &&
         !(CGM.getLangOpts().CUDA && CGM.getLangOpts().CUDAIsDevice) &&
         "Address space agnostic languages only");
  return D ? D->getType().getAddressSpace() : LangAS::Default;
}

StringRef
TargetCodeGenInfo::getLLVMSyncScopeStr(const LangOptions &LangOpts,
                                       SyncScope Scope,
                                       llvm::AtomicOrdering Ordering) const {
  return ""; /* default sync scope */
}

llvm::SyncScope::ID
TargetCodeGenInfo::getLLVMSyncScopeID(const LangOptions &LangOpts,
                                      SyncScope Scope,
                                      llvm::AtomicOrdering Ordering,
                                      llvm::LLVMContext &Ctx) const {
  return Ctx.getOrInsertSyncScopeID(
      getLLVMSyncScopeStr(LangOpts, Scope, Ordering));
}

void CodeGen::addAtomicIgnoreDenormalModeMetadata(CodeGenFunction &CGF,
                                                  llvm::Instruction &AtomicInst,
                                                  bool AllowHalf) {
  auto *RMW = dyn_cast<llvm::AtomicRMWInst>(&AtomicInst);
  if (!RMW || RMW->getOperation() != llvm::AtomicRMWInst::FAdd)
    return;

  llvm::Type *Ty = RMW->getType();
  if (!Ty->isFloatTy() && !(AllowHalf && Ty->isHalfTy()))
    return;

  RMW->setMetadata(llvm::LLVMContext::MD_atomic_ignore_denormal_mode,
                   llvm::MDNode::get(CGF.getLLVMContext(), {}));
}

void TargetCodeGenInfo::addStackProbeTargetAttributes(
    const Decl *D, llvm::GlobalValue *GV, CodeGen::CodeGenModule &CGM) const {
  if (llvm::Function *Fn = dyn_cast_or_null<llvm::Function>(GV)) {
    if (CGM.getCodeGenOpts().StackProbeSize != 4096)
      Fn->addFnAttr("stack-probe-size",
                    llvm::utostr(CGM.getCodeGenOpts().StackProbeSize));
    if (CGM.getCodeGenOpts().NoStackArgProbe)
      Fn->addFnAttr("no-stack-arg-probe");
  }
}

/// Create an OpenCL kernel for an enqueued block.
///
/// The kernel has the same function type as the block invoke function. Its
/// name is the name of the block invoke function postfixed with "_kernel".
/// It simply calls the block invoke function then returns.
llvm::Value *TargetCodeGenInfo::createEnqueuedBlockKernel(
    CodeGenFunction &CGF, llvm::Function *Invoke, llvm::Type *BlockTy) const {
  auto *InvokeFT = Invoke->getFunctionType();
  auto &C = CGF.getLLVMContext();
  std::string Name = Invoke->getName().str() + "_kernel";
  auto *FT = llvm::FunctionType::get(llvm::Type::getVoidTy(C),
                                     InvokeFT->params(), false);
  auto *F = llvm::Function::Create(FT, llvm::GlobalValue::ExternalLinkage, Name,
                                   &CGF.CGM.getModule());
  llvm::CallingConv::ID KernelCC =
      CGF.getTypes().ClangCallConvToLLVMCallConv(CallingConv::CC_DeviceKernel);
  F->setCallingConv(KernelCC);

  llvm::AttrBuilder KernelAttrs(C);

  // FIXME: This is missing setTargetAttributes
  CGF.CGM.addDefaultFunctionDefinitionAttributes(KernelAttrs);
  F->addFnAttrs(KernelAttrs);

  if (CGF.Builder.getIsFPConstrained())
    F->addFnAttr(llvm::Attribute::StrictFP);

  auto IP = CGF.Builder.saveIP();
  auto *BB = llvm::BasicBlock::Create(C, "entry", F);
  auto &Builder = CGF.Builder;
  Builder.SetInsertPoint(BB);
  llvm::SmallVector<llvm::Value *, 2> Args(llvm::make_pointer_range(F->args()));
  llvm::CallInst *Call = Builder.CreateCall(Invoke, Args);
  Call->setCallingConv(Invoke->getCallingConv());

  Builder.CreateRetVoid();
  Builder.restoreIP(IP);
  return F;
}

void TargetCodeGenInfo::setBranchProtectionFnAttributes(
    const TargetInfo::BranchProtectionInfo &BPI, llvm::Function &F) {
  // Called on already created and initialized function where attributes already
  // set from command line attributes but some might need to be removed as the
  // actual BPI is different.

  if (BPI.SignReturnAddr != LangOptions::SignReturnAddressScopeKind::None) {
    F.addFnAttr("sign-return-address", BPI.getSignReturnAddrStr());
    F.addFnAttr("sign-return-address-key", BPI.getSignKeyStr());
  } else {
    if (F.hasFnAttribute("sign-return-address"))
      F.removeFnAttr("sign-return-address");
    if (F.hasFnAttribute("sign-return-address-key"))
      F.removeFnAttr("sign-return-address-key");
  }

  if (BPI.SignReturnAddressHardening ==
      LangOptions::SignReturnAddressHardeningKind::None) {
    F.removeFnAttr("sign-return-address-harden");
  } else {
    F.addFnAttr("sign-return-address-harden",
                BPI.getSignReturnAddressHardeningStr());
  }

  auto AddRemoveAttributeAsSet = [&](bool Set, const StringRef &ModAttr) {
    if (Set)
      F.addFnAttr(ModAttr);
    else if (F.hasFnAttribute(ModAttr))
      F.removeFnAttr(ModAttr);
  };

  AddRemoveAttributeAsSet(BPI.BranchTargetEnforcement,
                          "branch-target-enforcement");
  AddRemoveAttributeAsSet(BPI.BranchProtectionPAuthLR,
                          "branch-protection-pauth-lr");
  AddRemoveAttributeAsSet(BPI.GuardedControlStack, "guarded-control-stack");
}

void TargetCodeGenInfo::initBranchProtectionFnAttributes(
    const TargetInfo::BranchProtectionInfo &BPI, llvm::AttrBuilder &FuncAttrs) {
  // Only used for initializing attributes in the AttrBuilder, which will not
  // contain any of these attributes so no need to remove anything.
  if (BPI.SignReturnAddr != LangOptions::SignReturnAddressScopeKind::None) {
    FuncAttrs.addAttribute("sign-return-address", BPI.getSignReturnAddrStr());
    FuncAttrs.addAttribute("sign-return-address-key", BPI.getSignKeyStr());
  }
  if (BPI.SignReturnAddressHardening !=
      LangOptions::SignReturnAddressHardeningKind::None)
    FuncAttrs.addAttribute("sign-return-address-harden",
                           BPI.getSignReturnAddressHardeningStr());
  if (BPI.BranchTargetEnforcement)
    FuncAttrs.addAttribute("branch-target-enforcement");
  if (BPI.BranchProtectionPAuthLR)
    FuncAttrs.addAttribute("branch-protection-pauth-lr");
  if (BPI.GuardedControlStack)
    FuncAttrs.addAttribute("guarded-control-stack");
}

void TargetCodeGenInfo::setPointerAuthFnAttributes(
    const PointerAuthOptions &Opts, llvm::Function &F) {
  auto UpdateAttr = [&F](bool AttrShouldExist, StringRef AttrName) {
    if (AttrShouldExist && !F.hasFnAttribute(AttrName))
      F.addFnAttr(AttrName);
    if (!AttrShouldExist && F.hasFnAttribute(AttrName))
      F.removeFnAttr(AttrName);
  };
  UpdateAttr(Opts.ReturnAddresses, "ptrauth-returns");
  UpdateAttr((bool)Opts.FunctionPointers, "ptrauth-calls");
  UpdateAttr(Opts.AuthTraps, "ptrauth-auth-traps");
  UpdateAttr(Opts.IndirectGotos, "ptrauth-indirect-gotos");
  UpdateAttr(Opts.AArch64JumpTableHardening, "aarch64-jump-table-hardening");
}

void TargetCodeGenInfo::initPointerAuthFnAttributes(
    const PointerAuthOptions &Opts, llvm::AttrBuilder &FuncAttrs) {
  if (Opts.ReturnAddresses)
    FuncAttrs.addAttribute("ptrauth-returns");
  if (Opts.FunctionPointers)
    FuncAttrs.addAttribute("ptrauth-calls");
  if (Opts.AuthTraps)
    FuncAttrs.addAttribute("ptrauth-auth-traps");
  if (Opts.IndirectGotos)
    FuncAttrs.addAttribute("ptrauth-indirect-gotos");
  if (Opts.AArch64JumpTableHardening)
    FuncAttrs.addAttribute("aarch64-jump-table-hardening");
}

namespace {
class DefaultTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  DefaultTargetCodeGenInfo(CodeGen::CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<DefaultABIInfo>(CGT)) {}
};
} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createDefaultTargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<DefaultTargetCodeGenInfo>(CGM.getTypes());
}

namespace {
/// i8086 (16-bit DOS). Global variables are in DGROUP, the near data
/// address space 0, in every memory model. In the far-data models C
/// pointers are far, and the addresses of globals are cast to them.
///
/// In the far-data models a variable larger than the data threshold
/// (-mlarge-data-threshold=, 32767 bytes by default, as in Open Watcom) is
/// far data instead, in a segment of its own outside DGROUP: address space
/// 2, or 3 (huge) in the huge models, where it can be over 64 KiB. So is
/// an extern variable of unknown size (extern char buf[]), which may be one.
///
/// The pointer to a returned aggregate is far in the far-data models, and in
/// every model on 16-bit Windows. The result may be on the stack, and in a
/// Windows DLL the stack segment is not DGROUP.
static bool isI8086FarDataModel(const TargetInfo &Target) {
  StringRef Model = Target.getABI();
  return Model == "compact" || Model == "large" || Model.starts_with("huge");
}

static bool hasI8086FarSRet(const TargetInfo &Target) {
  return isI8086FarDataModel(Target) || Target.getTriple().isOSWin16();
}

class I8086ABIInfo : public DefaultABIInfo {
public:
  I8086ABIInfo(CodeGen::CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  void computeInfo(CGFunctionInfo &FI) const override {
    DefaultABIInfo::computeInfo(FI);
    ABIArgInfo &Ret = FI.getReturnInfo();
    if (Ret.isIndirect() && hasI8086FarSRet(getContext().getTargetInfo()))
      Ret.setIndirectAddrSpace(2);
    // The structure parameter of an interrupt function (union INTPACK) is
    // the registers that its prologue saved, which the function changes for
    // the interrupted program: a pointer to them, not a copy, which the
    // backend takes as such (and reaches them through SS, as they are on
    // the interrupted program's stack).
    if (FI.getCallingConvention() == llvm::CallingConv::X86_INTR)
      for (auto &Arg : FI.arguments())
        if (isAggregateTypeForABI(Arg.type))
          Arg.info = ABIArgInfo::getIndirect(
              getContext().getTypeAlignInChars(Arg.type),
              getDataLayout().getAllocaAddrSpace(), /*ByVal=*/false);
  }
};

class I8086TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  I8086TargetCodeGenInfo(CodeGen::CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<I8086ABIInfo>(CGT)) {}

  LangAS getSRetAddrSpace(const CXXRecordDecl *RD) const override {
    if (hasI8086FarSRet(getABIInfo().getContext().getTargetInfo()))
      return getLangASFromTargetAS(2);
    return TargetCodeGenInfo::getSRetAddrSpace(RD);
  }

  LangAS getGlobalVarAddressSpace(CodeGenModule &CGM,
                                  const VarDecl *D) const override {
    LangAS AS = D ? D->getType().getAddressSpace() : LangAS::Default;
    if (AS != LangAS::Default)
      return AS;
    if (D && isFarVariable(CGM, D)) {
      bool Huge = CGM.getTarget().getABI().starts_with("huge");
      return getLangASFromTargetAS(Huge ? 3 : 2);
    }
    return getLangASFromTargetAS(0);
  }

  // __loadds, and the 16-bit Windows entry sequence of -mwindows-prologue=.
  // The backend gives the Windows sequence to far functions only.
  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &CGM) const override {
    const auto *FD = dyn_cast_or_null<FunctionDecl>(D);
    auto *Fn = dyn_cast<llvm::Function>(GV);
    if (!FD || !Fn)
      return;
    if (FD->hasAttr<I8086LoadDSAttr>())
      Fn->addFnAttr("i8086-loadds");
    StringRef Prologue = CGM.getCodeGenOpts().WindowsPrologue;
    if (Prologue == "all" ||
        (Prologue == "exported" && FD->hasAttr<DLLExportAttr>()))
      Fn->addFnAttr("i8086-windows-prologue");
    // A DLL runs on its caller's stack, so a near pointer, which means DS,
    // cannot point to a local. In the near data models the backend moves
    // the locals whose address escapes to a frame in DGROUP, with the
    // SafeStack pass.
    if (CGM.getCodeGenOpts().WindowsDLL &&
        !isI8086FarDataModel(CGM.getTarget()))
      Fn->addFnAttr(llvm::Attribute::SafeStack);
  }

private:
  static bool isFarVariable(CodeGenModule &CGM, const VarDecl *D) {
    uint64_t Threshold = CGM.getCodeGenOpts().LargeDataThreshold;
    if (!Threshold || !isI8086FarDataModel(CGM.getTarget()))
      return false;
    // Use the definition, or the declaration with the most type information.
    if (const VarDecl *Def = D->getDefinition())
      D = Def;
    else
      D = D->getMostRecentDecl();
    QualType T = D->getType();
    if (T->isDependentType())
      return false;
    // A variable of unknown size declared here may be defined elsewhere as
    // far data. A far reference reaches a near variable too, so it is far.
    if (T->isIncompleteType())
      return D->hasExternalStorage();
    if (!T->isConstantSizeType())
      return false;
    return CGM.getContext().getTypeSizeInChars(T).getQuantity() >
           int64_t(Threshold);
  }
};
} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createI8086TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<I8086TargetCodeGenInfo>(CGM.getTypes());
}
