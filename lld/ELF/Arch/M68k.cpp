//===- M68k.cpp -----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Motorola 68000 family. Only static linking is supported: absolute and
// PC-relative relocations, with calls through the PLT resolved directly.
// This is what AmigaOS and classic Mac OS programs need; they are converted
// to the OS's own executable format after linking.
//
//===----------------------------------------------------------------------===//

#include "Symbols.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
class M68k final : public TargetInfo {
public:
  M68k(Ctx &);
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  int64_t getImplicitAddend(const uint8_t *buf, RelType type) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};
} // namespace

M68k::M68k(Ctx &ctx) : TargetInfo(ctx) {
  // illegal
  trapInstr = {0x4a, 0xfc, 0x4a, 0xfc};
  defaultMaxPageSize = 0x2000;
  defaultCommonPageSize = 0x2000;
}

RelExpr M68k::getRelExpr(RelType type, const Symbol &s,
                         const uint8_t *loc) const {
  switch (type) {
  case R_68K_NONE:
    return R_NONE;
  case R_68K_32:
  case R_68K_16:
  case R_68K_8:
    return R_ABS;
  case R_68K_PC32:
  case R_68K_PC16:
  case R_68K_PC8:
    return R_PC;
  case R_68K_PLT32:
  case R_68K_PLT16:
  case R_68K_PLT8:
    return R_PLT_PC;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unknown relocation (" << type.v
             << ") against symbol " << &s;
    return R_NONE;
  }
}

int64_t M68k::getImplicitAddend(const uint8_t *buf, RelType type) const {
  switch (type) {
  case R_68K_NONE:
    return 0;
  case R_68K_32:
  case R_68K_PC32:
  case R_68K_PLT32:
    return SignExtend64<32>(read32be(buf));
  case R_68K_16:
  case R_68K_PC16:
  case R_68K_PLT16:
    return SignExtend64<16>(read16be(buf));
  case R_68K_8:
  case R_68K_PC8:
  case R_68K_PLT8:
    return SignExtend64<8>(*buf);
  default:
    InternalErr(ctx, buf) << "cannot read addend for relocation " << type;
    return 0;
  }
}

void M68k::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_68K_32:
  case R_68K_PC32:
  case R_68K_PLT32:
    write32be(loc, val);
    break;
  case R_68K_16:
    checkIntUInt(ctx, loc, val, 16, rel);
    write16be(loc, val);
    break;
  case R_68K_PC16:
  case R_68K_PLT16:
    checkInt(ctx, loc, val, 16, rel);
    write16be(loc, val);
    break;
  case R_68K_8:
    checkIntUInt(ctx, loc, val, 8, rel);
    *loc = val;
    break;
  case R_68K_PC8:
  case R_68K_PLT8:
    checkInt(ctx, loc, val, 8, rel);
    *loc = val;
    break;
  default:
    llvm_unreachable("unknown relocation");
  }
}

void elf::setM68kTargetInfo(Ctx &ctx) { ctx.target.reset(new M68k(ctx)); }
