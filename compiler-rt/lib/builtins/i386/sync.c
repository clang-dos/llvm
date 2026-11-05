//===-- sync.c - __sync_* atomics for the 80386 ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The 80386 has no XADD or CMPXCHG, so code built for it (-march=i386) calls
// these functions for atomic writes of 1, 2 and 4 bytes. On an 80486 or later
// they use those instructions. On an 80386 they serialize every atomic write
// through one spinlock. Atomic loads stay plain MOVs, which works because
// every write, atomic stores included, is a single store made under the lock.
//
//===----------------------------------------------------------------------===//

#include <stdint.h>

#if defined(__i386__)

#define STR2(x) #x
#define STR(x) STR2(x)
#define SYMBOL(name) __asm__(STR(__USER_LABEL_PREFIX__) #name)

#ifdef _WIN32
__declspec(dllimport) void __stdcall Sleep(unsigned long);
#endif

// 0 until checked, then 1 on an 80386 and 2 on an 80486 or later.
static volatile int cpu_level;

// The 80486 added the AC flag, bit 18 of EFLAGS. The 80386 keeps it at 0.
static int has_486(void) {
  int level = cpu_level;
  if (level == 0) {
    uint32_t changed;
    __asm__ volatile("pushfl\n\t"
                     "popl %%eax\n\t"
                     "movl %%eax, %%ecx\n\t"
                     "xorl $0x40000, %%eax\n\t"
                     "pushl %%eax\n\t"
                     "popfl\n\t"
                     "pushfl\n\t"
                     "popl %%eax\n\t"
                     "pushl %%ecx\n\t"
                     "popfl\n\t"
                     "xorl %%ecx, %%eax"
                     : "=a"(changed)
                     :
                     : "ecx", "cc");
    level = (changed & 0x40000) ? 2 : 1;
    cpu_level = level;
  }
  return level == 2;
}

static volatile uint32_t lock_word;

static void lock(void) {
  unsigned waits = 0;
  for (;;) {
    uint32_t old = 1;
    // XCHG with memory is atomic on every x86.
    __asm__ volatile("xchgl %0, %1"
                     : "+r"(old), "+m"(lock_word)
                     :
                     : "memory");
    if (old == 0)
      return;
    while (lock_word) {
#ifdef _WIN32
      // A uniprocessor 80386 only gets the lock back once its owner runs.
      Sleep(waits < 16 ? 0 : 1);
      ++waits;
#else
      __asm__ volatile("rep; nop" ::: "memory");
#endif
    }
  }
}

static void unlock(void) {
  __asm__ volatile("" ::: "memory");
  lock_word = 0;
}

#define CAS(SUFFIX, T)                                                         \
  static T cas_##T(volatile T *p, T expected, T desired) {                     \
    if (has_486()) {                                                           \
      __asm__ volatile("lock; cmpxchg" SUFFIX " %2, %1"                        \
                       : "+a"(expected), "+m"(*p)                              \
                       : "q"(desired)                                          \
                       : "memory", "cc");                                      \
      return expected;                                                         \
    }                                                                          \
    lock();                                                                    \
    T old = *p;                                                                \
    if (old == expected)                                                       \
      *p = desired;                                                            \
    unlock();                                                                  \
    return old;                                                                \
  }

CAS("b", uint8_t)
CAS("w", uint16_t)
CAS("l", uint32_t)

#define XCHG(SUFFIX, T)                                                        \
  static T xchg_##T(volatile T *p, T value) {                                  \
    if (has_486()) {                                                           \
      __asm__ volatile("xchg" SUFFIX " %0, %1"                                 \
                       : "+q"(value), "+m"(*p)                                 \
                       :                                                       \
                       : "memory");                                            \
      return value;                                                            \
    }                                                                          \
    lock();                                                                    \
    T old = *p;                                                                \
    *p = value;                                                                \
    unlock();                                                                  \
    return old;                                                                \
  }

XCHG("b", uint8_t)
XCHG("w", uint16_t)
XCHG("l", uint32_t)

#define XADD(SUFFIX, T)                                                        \
  static T xadd_##T(volatile T *p, T value) {                                  \
    if (has_486()) {                                                           \
      __asm__ volatile("lock; xadd" SUFFIX " %0, %1"                           \
                       : "+q"(value), "+m"(*p)                                 \
                       :                                                       \
                       : "memory", "cc");                                      \
      return value;                                                            \
    }                                                                          \
    lock();                                                                    \
    T old = *p;                                                                \
    *p = (T)(old + value);                                                     \
    unlock();                                                                  \
    return old;                                                                \
  }

XADD("b", uint8_t)
XADD("w", uint16_t)
XADD("l", uint32_t)

#define DEFINE(N, T, S)                                                        \
  T val_compare_and_swap_##N(volatile T *p, T expected, T desired)             \
      SYMBOL(__sync_val_compare_and_swap_##N);                                 \
  T val_compare_and_swap_##N(volatile T *p, T expected, T desired) {           \
    return cas_##T(p, expected, desired);                                      \
  }                                                                            \
  T lock_test_and_set_##N(volatile T *p, T value)                              \
      SYMBOL(__sync_lock_test_and_set_##N);                                    \
  T lock_test_and_set_##N(volatile T *p, T value) {                            \
    return xchg_##T(p, value);                                                 \
  }                                                                            \
  T fetch_and_add_##N(volatile T *p, T value)                                  \
      SYMBOL(__sync_fetch_and_add_##N);                                        \
  T fetch_and_add_##N(volatile T *p, T value) { return xadd_##T(p, value); }   \
  T fetch_and_sub_##N(volatile T *p, T value)                                  \
      SYMBOL(__sync_fetch_and_sub_##N);                                        \
  T fetch_and_sub_##N(volatile T *p, T value) {                                \
    return xadd_##T(p, (T)-value);                                             \
  }                                                                            \
  OP(N, T, S, and, old & value)                                                \
  OP(N, T, S, or, old | value)                                                 \
  OP(N, T, S, xor, old ^ value)                                                \
  OP(N, T, S, nand, ~(old & value))                                            \
  OP(N, T, S, umax, old > value ? old : value)                                 \
  OP(N, T, S, umin, old < value ? old : value)                                 \
  OP(N, T, S, max, (S)old > (S)value ? old : value)                            \
  OP(N, T, S, min, (S)old < (S)value ? old : value)

// The other operations retry a compare-and-swap until no other write came in
// between.
#define OP(N, T, S, NAME, EXPR)                                                \
  T fetch_and_##NAME##_##N(volatile T *p, T value)                             \
      SYMBOL(__sync_fetch_and_##NAME##_##N);                                   \
  T fetch_and_##NAME##_##N(volatile T *p, T value) {                           \
    T old = *p;                                                                \
    for (;;) {                                                                 \
      T seen = cas_##T(p, old, (T)(EXPR));                                     \
      if (seen == old)                                                         \
        return old;                                                            \
      old = seen;                                                              \
    }                                                                          \
  }

DEFINE(1, uint8_t, int8_t)
DEFINE(2, uint16_t, int16_t)
DEFINE(4, uint32_t, int32_t)

#endif // __i386__
