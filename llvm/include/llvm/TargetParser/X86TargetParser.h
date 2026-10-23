//===-- X86TargetParser - Parser for X86 features ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a target parser to recognise X86 hardware features.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGETPARSER_X86TARGETPARSER_H
#define LLVM_TARGETPARSER_X86TARGETPARSER_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/Compiler.h"
#include <array>
#include <optional>

namespace llvm {
template <typename T> class SmallVectorImpl;
class StringRef;

namespace X86 {

enum ProcessorVendors : unsigned {
#define X86_VENDOR(ENUM, STRING, ABI_VALUE) ENUM,
#include "llvm/TargetParser/X86TargetParser.def"
  CPU_VENDOR_MAX
};

enum ProcessorTypes : unsigned {
#define X86_CPU_TYPE(ENUM, STRING, ABI_VALUE) ENUM,
#include "llvm/TargetParser/X86TargetParser.def"
  CPU_TYPE_MAX
};

enum ProcessorSubtypes : unsigned {
#define X86_CPU_SUBTYPE(ENUM, STRING, ABI_VALUE) ENUM,
#include "llvm/TargetParser/X86TargetParser.def"
  CPU_SUBTYPE_MAX
};

enum ProcessorFeatures {
#define X86_FEATURE(ENUM, STRING) FEATURE_##ENUM,
#include "llvm/TargetParser/X86TargetParser.def"
  CPU_FEATURE_MAX,
#define X86_MICROARCH_LEVEL(ENUM, STR, PRIORITY, ABI_VALUE) FEATURE_##ENUM,
#include "llvm/TargetParser/X86TargetParser.def"
};

enum CPUKind {
  CK_None,
  // 16-bit CPUs, only valid for the i8086 triple.
  CK_i8086,
  CK_i8088,
  CK_i186,
  CK_V20,
  CK_V30,
  CK_i286,
  CK_i386,
  CK_i486,
  CK_WinChipC6,
  CK_WinChip2,
  CK_C3,
  CK_i586,
  CK_Pentium,
  CK_PentiumMMX,
  CK_PentiumPro,
  CK_i686,
  CK_Pentium2,
  CK_Pentium3,
  CK_PentiumM,
  CK_C3_2,
  CK_Yonah,
  CK_Pentium4,
  CK_Prescott,
  CK_Nocona,
  CK_Core2,
  CK_Penryn,
  CK_Bonnell,
  CK_Silvermont,
  CK_Goldmont,
  CK_GoldmontPlus,
  CK_Tremont,
  CK_Gracemont,
  CK_Nehalem,
  CK_Westmere,
  CK_SandyBridge,
  CK_IvyBridge,
  CK_Haswell,
  CK_Broadwell,
  CK_SkylakeClient,
  CK_SkylakeServer,
  CK_Cascadelake,
  CK_Cooperlake,
  CK_Cannonlake,
  CK_IcelakeClient,
  CK_Rocketlake,
  CK_IcelakeServer,
  CK_Tigerlake,
  CK_SapphireRapids,
  CK_Alderlake,
  CK_Raptorlake,
  CK_Meteorlake,
  CK_Arrowlake,
  CK_ArrowlakeS,
  CK_Lunarlake,
  CK_Pantherlake,
  CK_Wildcatlake,
  CK_Novalake,
  CK_Sierraforest,
  CK_Grandridge,
  CK_Graniterapids,
  CK_GraniterapidsD,
  CK_Emeraldrapids,
  CK_Clearwaterforest,
  CK_Diamondrapids,
  CK_KNL,
  CK_KNM,
  CK_Lakemont,
  CK_K6,
  CK_K6_2,
  CK_K6_3,
  CK_Athlon,
  CK_AthlonXP,
  CK_K8,
  CK_K8SSE3,
  CK_AMDFAM10,
  CK_BTVER1,
  CK_BTVER2,
  CK_BDVER1,
  CK_BDVER2,
  CK_BDVER3,
  CK_BDVER4,
  CK_ZNVER1,
  CK_ZNVER2,
  CK_ZNVER3,
  CK_ZNVER4,
  CK_ZNVER5,
  CK_ZNVER6,
  CK_C86_4G_M4,
  CK_C86_4G_M6,
  CK_C86_4G_M7,
  CK_C86_4G_M8,
  CK_x86_64,
  CK_x86_64_v2,
  CK_x86_64_v3,
  CK_x86_64_v4,
  CK_Geode,
};

/// Parse \p CPU string into a CPUKind. Will only accept 64-bit capable CPUs if
/// \p Only64Bit is true. CPUs older than the 80386 are only accepted if
/// \p Allow16Bit is true, which is the case for the i8086 triple.
LLVM_ABI CPUKind parseArchX86(StringRef CPU, bool Only64Bit = false,
                              bool Allow16Bit = false);
LLVM_ABI CPUKind parseTuneCPU(StringRef CPU, bool Only64Bit = false,
                              bool Allow16Bit = false);

/// Provide a list of valid CPU names. If \p Only64Bit is true, the list will
/// only contain 64-bit capable CPUs. CPUs older than the 80386 are only listed
/// if \p Allow16Bit is true.
LLVM_ABI void fillValidCPUArchList(SmallVectorImpl<StringRef> &Values,
                                   bool Only64Bit = false,
                                   bool Allow16Bit = false);
/// Provide a list of valid -mtune names.
LLVM_ABI void fillValidTuneCPUList(SmallVectorImpl<StringRef> &Values,
                                   bool Only64Bit = false,
                                   bool Allow16Bit = false);

/// Get the key feature prioritizing target multiversioning.
LLVM_ABI ProcessorFeatures getKeyFeature(CPUKind Kind);

/// Fill in the features that \p CPU supports into \p Features.
/// "+" will be append in front of each feature if NeedPlus is true.
LLVM_ABI void getFeaturesForCPU(StringRef CPU,
                                SmallVectorImpl<StringRef> &Features,
                                bool NeedPlus = false);

/// Set or clear entries in \p Features that are implied to be enabled/disabled
/// by the provided \p Feature.
LLVM_ABI void updateImpliedFeatures(StringRef Feature, bool Enabled,
                                    StringMap<bool> &Features);

LLVM_ABI char getCPUDispatchMangling(StringRef Name);
LLVM_ABI bool validateCPUSpecificCPUDispatch(StringRef Name);
LLVM_ABI std::array<uint32_t, 4>
getCpuSupportsMask(ArrayRef<StringRef> FeatureStrs);
LLVM_ABI unsigned getFeaturePriority(ProcessorFeatures Feat);

/// Whether \p Kind is a CPU older than the 80386, which only the i8086 triple
/// supports.
inline bool is16BitCPU(CPUKind Kind) {
  return Kind >= CK_i8086 && Kind <= CK_i286;
}

/// Memory models of the 16-bit real-mode (i8086) target. The memory model
/// decides the default size of code and data pointers, and so the data
/// layout. It is passed as the target ABI name.
enum class MemoryModel16 {
  Tiny,    // Near code and data, all in one 64 KB group (.COM files).
  Small,   // Near code, near data.
  Medium,  // Far code, near data.
  Compact, // Near code, far data.
  Large,   // Far code, far data.
  Huge,    // Far code, huge (normalized) data.
};

/// Parse an i8086 memory model name ("tiny", "small", ...). An empty name
/// means the default, small. "huge16" is the huge model with a 16-bit size_t.
/// Returns std::nullopt for an unknown name.
LLVM_ABI std::optional<MemoryModel16> parseMemoryModel16(StringRef Name);

/// The width of size_t in bits for an i8086 ABI name: 32 in the huge model,
/// and 16 in the others and in "huge16", which follows Open Watcom.
LLVM_ABI unsigned getSizeTWidth16(StringRef ABIName);

/// Returns the canonical name of \p Model.
LLVM_ABI StringRef getMemoryModel16Name(MemoryModel16 Model);

/// Whether code pointers and calls are far (32-bit segment:offset) by default.
inline bool isFarCodeModel16(MemoryModel16 Model) {
  return Model == MemoryModel16::Medium || Model == MemoryModel16::Large ||
         Model == MemoryModel16::Huge;
}

/// Whether data pointers are far or huge (32-bit) by default.
inline bool isFarDataModel16(MemoryModel16 Model) {
  return Model == MemoryModel16::Compact || Model == MemoryModel16::Large ||
         Model == MemoryModel16::Huge;
}

/// Address spaces of the i8086 target. Address space 0 means the default data
/// pointer of the current memory model.
enum AddressSpace16 : unsigned {
  AS16_Default = 0,
  AS16_NearData = 1, // 16-bit offset in DGROUP (DS-relative).
  AS16_FarData = 2,  // 32-bit seg:off, arithmetic on the offset only.
  AS16_HugeData = 3, // 32-bit seg:off, normalized, 20-bit arithmetic.
  AS16_NearCode = 4, // 16-bit CS-relative offset; near functions.
  AS16_FarCode = 5,  // 32-bit seg:off of a far function.
};

} // namespace X86
} // namespace llvm

#endif
