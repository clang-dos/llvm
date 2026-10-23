//===-- lshrsi3.c - Implement __lshrsi3 -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements __lshrsi3 for the compiler_rt library. Targets with
// 16-bit registers, such as i8086, call it for 32-bit shifts.
//
//===----------------------------------------------------------------------===//

#include "int_lib.h"

// Returns: logical a >> b

// Precondition:  0 <= b < bits_in_word

COMPILER_RT_ABI si_int __lshrsi3(si_int a, int b) {
  const int bits_in_hword = (int)(sizeof(hi_int) * CHAR_BIT);
  uswords input;
  uswords result;
  input.all = (su_int)a;
  if (b & bits_in_hword) /* bits_in_hword <= b < bits_in_word */ {
    result.s.high = 0;
    result.s.low = input.s.high >> (b - bits_in_hword);
  } else /* 0 <= b < bits_in_hword */ {
    if (b == 0)
      return a;
    result.s.high = input.s.high >> b;
    result.s.low =
        (hu_int)(input.s.high << (bits_in_hword - b)) | (input.s.low >> b);
  }
  return (si_int)result.all;
}
