//===-- OMFDump.cpp - OMF-specific dumper -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements the OMF-specific dumper for llvm-objdump.
///
//===----------------------------------------------------------------------===//

#include "OMFDump.h"
#include "llvm-objdump.h"
#include "llvm/Object/OMFObjectFile.h"

using namespace llvm;
using namespace llvm::object;

namespace {
class OMFDumper : public objdump::Dumper {
  const OMFObjectFile &Obj;

public:
  OMFDumper(const OMFObjectFile &O) : Dumper(O), Obj(O) {}
  void printPrivateHeaders() override;
};
} // namespace

void OMFDumper::printPrivateHeaders() {
  outs() << "Module: " << Obj.getModuleName() << "\n";
  outs() << "Segments:\n";
  for (const OMFObjectFile::Segment &S : Obj.segments()) {
    outs() << "  " << S.Name << " class=" << S.Class;
    if (!S.Group.empty())
      outs() << " group=" << S.Group;
    outs() << " size=" << format_hex(S.Size, 6)
           << " align=" << (S.Align ? unsigned(S.Align) : 256u)
           << " combine=" << unsigned(S.Combine) << (S.Use32 ? " use32" : "")
           << "\n";
  }
}

std::unique_ptr<objdump::Dumper>
objdump::createOMFDumper(const OMFObjectFile &Obj) {
  return std::make_unique<OMFDumper>(Obj);
}

Error objdump::getOMFRelocationValueString(const OMFObjectFile &Obj,
                                           const RelocationRef &Rel,
                                           SmallVectorImpl<char> &Result) {
  DataRefImpl D = Rel.getRawDataRefImpl();
  const OMFObjectFile::Relocation &R = Obj.segments()[D.d.a].Relocs[D.d.b];
  Result.append(R.TargetName.begin(), R.TargetName.end());
  return Error::success();
}
