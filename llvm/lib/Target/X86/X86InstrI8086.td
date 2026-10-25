//===-- X86InstrI8086.td - 8086/80186/80286 code generation -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Selection patterns for CPUs older than the 80386, which have no 32-bit
// registers, MOVZX/MOVSX, SETcc or shifts by an immediate other than 1 (the
// 80186 added those). Only the i8086 triple selects these CPUs. The patterns
// have a high AddedComplexity so that they win over the generic patterns,
// which use 80386 instructions for the same DAGs.
//
//===----------------------------------------------------------------------===//

let Predicates = [IsPre386], AddedComplexity = 100 in {

// Zero extension from i8 clears the high byte. The source must be in a
// register with an 8-bit low half (AX, BX, CX or DX).
def : Pat<(i16 (zext GR8:$src)),
          (AND16ri (INSERT_SUBREG (i16 (IMPLICIT_DEF)), GR8:$src, sub_8bit),
                   0xff)>;
def : Pat<(i16 (anyext GR8:$src)),
          (INSERT_SUBREG (i16 (IMPLICIT_DEF)), GR8:$src, sub_8bit)>;

def : Pat<(zextloadi16i8 addr:$src),
          (INSERT_SUBREG (i16 (COPY_TO_REGCLASS (MOV16r0), GR16)),
                         (MOV8rm addr:$src), sub_8bit)>;
def : Pat<(zextloadi16i1 addr:$src),
          (INSERT_SUBREG (i16 (COPY_TO_REGCLASS (MOV16r0), GR16)),
                         (MOV8rm addr:$src), sub_8bit)>;
def : Pat<(extloadi16i8 addr:$src),
          (INSERT_SUBREG (i16 (IMPLICIT_DEF)), (MOV8rm addr:$src), sub_8bit)>;
def : Pat<(extloadi16i1 addr:$src),
          (INSERT_SUBREG (i16 (IMPLICIT_DEF)), (MOV8rm addr:$src), sub_8bit)>;

// r & 0xff is an AND, not MOVZX.
def : Pat<(and GR16:$src, 0xff), (AND16ri GR16:$src, 0xff)>;

// The high byte of a 16-bit register.
def : Pat<(srl GR16:$src, (i8 8)),
          (AND16ri (INSERT_SUBREG (i16 (IMPLICIT_DEF)),
                                  (EXTRACT_SUBREG GR16:$src, sub_8bit_hi),
                                  sub_8bit), 0xff)>;
def : Pat<(i8 (trunc (srl GR16:$src, (i8 8)))),
          (EXTRACT_SUBREG GR16:$src, sub_8bit_hi)>;

// There is no SETcc. Select 0 or 1 with a branch, which the CMOV_GR8 custom
// inserter emits.
def : Pat<(X86setcc timm:$cond, EFLAGS),
          (CMOV_GR8 (MOV8ri 0), (MOV8ri 1), timm:$cond)>;

// UD2 needs an 80286 or later, and to an 8086 0F is POP CS. Trap with INT3 in
// a loop instead, so a program without a debugger stops there.
let isTrap = 1, isBarrier = 1, hasSideEffects = 1, mayLoad = 1,
    SchedRW = [WriteSystem] in
def TRAP_8086 : PseudoI<(outs), (ins), [(trap)]>;
def : Pat<(ubsantrap (i32 timm)), (TRAP_8086)>;

} // Predicates = [IsPre386], AddedComplexity = 100

// The 8086 can only shift by 1 or by CL. Shifts by other constants fall back
// to the CL forms, but shifting by 1 two or three times is as small and
// faster.
let Predicates = [No186], AddedComplexity = 100 in {
multiclass Shift8086<SDNode node, Instruction Op8, Instruction Op16> {
  def : Pat<(node GR8:$src, (i8 2)), (Op8 (Op8 GR8:$src))>;
  def : Pat<(node GR8:$src, (i8 3)), (Op8 (Op8 (Op8 GR8:$src)))>;
  def : Pat<(node GR16:$src, (i8 2)), (Op16 (Op16 GR16:$src))>;
  def : Pat<(node GR16:$src, (i8 3)), (Op16 (Op16 (Op16 GR16:$src)))>;
}
defm : Shift8086<shl, SHL8r1, SHL16r1>;
defm : Shift8086<srl, SHR8r1, SHR16r1>;
defm : Shift8086<sra, SAR8r1, SAR16r1>;
defm : Shift8086<rotl, ROL8r1, ROL16r1>;
defm : Shift8086<rotr, ROR8r1, ROR16r1>;
} // Predicates = [No186], AddedComplexity = 100

// Addresses of globals, constant pools, jump tables and so on are 16-bit
// immediates in the 16-bit (i8086) ABI.
let Predicates = [In16BitABIMode] in {
// LEA of a 16-bit address, for example the address of a stack object.
let AddedComplexity = 100 in
def : Pat<(i16 lea16addr:$src), (LEA16r lea16addr:$src)>;
def : Pat<(i16 relocImm:$src), (MOV16ri relocImm:$src)>;
def : Pat<(store (i16 relocImm16_su:$src), addr:$dst),
          (MOV16mi addr:$dst, relocImm16_su:$src)>;
}

// The relaxed form of JCC_1 before the 80386, which has no Jcc rel16: an
// inverted short Jcc over a near JMP. Only the assembler backend creates it,
// and the code emitter encodes it.
let isBranch = 1, isTerminator = 1, Uses = [EFLAGS], SchedRW = [WriteJump],
    isCodeGenOnly = 1, hasSideEffects = 0 in
def JCC_8086 : PseudoI<(outs), (ins brtarget16:$dst, ccode:$cond), []>;

// Far calls, for the memory models with far code. FARCALL16sym becomes
// FARCALL16i with the symbol's offset and segment (sym@SEG).
let isCall = 1, Uses = [SP, SSP], isCodeGenOnly = 1 in
def FARCALL16sym : PseudoI<(outs), (ins i16imm:$dst), []>,
                   Sched<[WriteJump]>;
// A call to an interrupt function: PUSHF, then the far call.
let isCall = 1, Uses = [SP, SSP], isCodeGenOnly = 1 in {
def FARCALL16sym_INTR : PseudoI<(outs), (ins i16imm:$dst), []>,
                        Sched<[WriteJump]>;
let mayLoad = 1 in
def FARCALL16m_INTR : PseudoI<(outs), (ins opaquemem:$dst), []>,
                      Sched<[WriteJumpLd]>;
}
let Predicates = [In16BitABIMode] in {
let AddedComplexity = 20 in {
def : Pat<(X86farcall_intr (i16 tglobaladdr:$dst)),
          (FARCALL16sym_INTR tglobaladdr:$dst)>;
def : Pat<(X86farcall_intr (i16 texternalsym:$dst)),
          (FARCALL16sym_INTR texternalsym:$dst)>;
}
def : Pat<(X86farcall_intr addr:$dst), (FARCALL16m_INTR addr:$dst)>;
}
let Predicates = [In16BitABIMode] in {
let AddedComplexity = 20 in {
def : Pat<(X86farcall (i16 tglobaladdr:$dst)), (FARCALL16sym tglobaladdr:$dst)>;
def : Pat<(X86farcall (i16 texternalsym:$dst)), (FARCALL16sym texternalsym:$dst)>;
}
def : Pat<(X86farcall addr:$dst), (FARCALL16m addr:$dst)>;
}
