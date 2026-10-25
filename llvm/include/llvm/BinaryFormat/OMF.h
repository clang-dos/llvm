//===-- llvm/BinaryFormat/OMF.h - OMF constants -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Constants for the Intel/Microsoft Relocatable Object Module Format (OMF),
// the 16-bit object format used by DOS linkers.
//
// Reference: "Tool Interface Standard (TIS) Relocatable Object Module Format
// (OMF) Specification", Version 1.1.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BINARYFORMAT_OMF_H
#define LLVM_BINARYFORMAT_OMF_H

#include <cstdint>

namespace llvm {
namespace OMF {

/// Record types. The 16-bit forms are used; 32-bit forms are the 16-bit form
/// plus one.
enum RecordType : uint8_t {
  THEADR = 0x80,
  COMENT = 0x88,
  MODEND = 0x8A,
  EXTDEF = 0x8C,
  PUBDEF = 0x90,
  LNAMES = 0x96,
  SEGDEF = 0x98,
  GRPDEF = 0x9A,
  FIXUPP = 0x9C,
  LEDATA = 0xA0,
  COMDEF = 0xB0,
  LEXTDEF = 0xB4,
  COMDAT = 0xC2,
  ALIAS = 0xC6,
};

/// Comment classes for COMENT records.
enum CommentClass : uint8_t {
  CC_Translator = 0x00,
  CC_OMFExtension = 0xA0, ///< OMF extensions; subtype 2 is EXPDEF (export).
  CC_WeakExtern = 0xA8, ///< WKEXT: weak external with a default resolution.
};

/// SEGDEF alignment field (the "A" in ACBP).
enum SegmentAlign : uint8_t {
  SA_Absolute = 0,
  SA_Byte = 1,
  SA_Word = 2,
  SA_Para = 3,
  SA_Page = 4,
};

/// SEGDEF combination field (the "C" in ACBP).
enum SegmentCombine : uint8_t {
  SC_Private = 0,
  SC_Public = 2,
  SC_Stack = 5,
  SC_Common = 6,
};

/// FIXUPP location types.
enum LocationType : uint8_t {
  LT_LowByte = 0,
  LT_Offset16 = 1,
  LT_Base16 = 2,
  LT_Pointer32 = 3,
  LT_HighByte = 4,
};

/// Frame methods.
enum FrameMethod : uint8_t {
  F_Segment = 0,
  F_Group = 1,
  F_External = 2,
  F_Location = 4,
  F_Target = 5,
};

/// Target methods. Methods 4-7 are methods 0-3 without a displacement field.
enum TargetMethod : uint8_t {
  T_Segment = 0,
  T_Group = 1,
  T_External = 2,
  T_SegmentNoDisp = 4,
  T_GroupNoDisp = 5,
  T_ExternalNoDisp = 6,
};

/// COMDEF data types.
enum CommunalType : uint8_t {
  CT_Far = 0x61,
  CT_Near = 0x62,
};

/// Linkers commonly reject records longer than this, so data and name lists
/// are split to stay below it.
constexpr unsigned MaxRecordData = 1024;

} // end namespace OMF
} // end namespace llvm

#endif // LLVM_BINARYFORMAT_OMF_H
