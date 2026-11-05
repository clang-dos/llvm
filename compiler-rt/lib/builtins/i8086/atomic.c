//===-- atomic.c - __atomic_* functions for 16-bit x86 --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Before the 80486 there is no XADD or CMPXCHG, so 16-bit code for an 8086,
// 286 or 386 calls these functions for every atomic operation. DOS and 16-bit
// Windows programs run on one processor without preemptive threads, so only
// an interrupt handler can come in between: each function turns interrupts
// off while it works.
//
//===----------------------------------------------------------------------===//

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline uint16_t interrupts_off(void) {
  uint16_t flags;
  __asm__ volatile("pushfw\n\t"
                   "popw %0\n\t"
                   "cli"
                   : "=r"(flags)
                   :
                   : "memory");
  return flags;
}

static inline void interrupts_restore(uint16_t flags) {
  __asm__ volatile("pushw %0\n\t"
                   "popfw"
                   :
                   : "r"(flags)
                   : "memory", "cc");
}

static void copy(volatile char *dst, const volatile char *src, size_t size) {
  while (size--)
    *dst++ = *src++;
}

// The functions are builtins in clang, so they are defined under other names.
#define STR2(x) #x
#define STR(x) STR2(x)
#define SYMBOL(name) __asm__(STR(__USER_LABEL_PREFIX__) #name)

#define FETCH(N, T, NAME, EXPR)                                                \
  T fetch_##NAME##_##N(volatile void *p, T value, int order)                   \
      SYMBOL(__atomic_fetch_##NAME##_##N);                                     \
  T fetch_##NAME##_##N(volatile void *p, T value, int order) {                 \
    (void)order;                                                               \
    volatile T *q = (volatile T *)p;                                           \
    uint16_t flags = interrupts_off();                                         \
    T old = *q;                                                                \
    *q = (T)(EXPR);                                                            \
    interrupts_restore(flags);                                                 \
    return old;                                                                \
  }

#define SIZED(N, T)                                                            \
  T load_##N(const volatile void *p, int order) SYMBOL(__atomic_load_##N);     \
  T load_##N(const volatile void *p, int order) {                              \
    (void)order;                                                               \
    uint16_t flags = interrupts_off();                                         \
    T value = *(const volatile T *)p;                                          \
    interrupts_restore(flags);                                                 \
    return value;                                                              \
  }                                                                            \
  void store_##N(volatile void *p, T value, int order)                         \
      SYMBOL(__atomic_store_##N);                                              \
  void store_##N(volatile void *p, T value, int order) {                       \
    (void)order;                                                               \
    uint16_t flags = interrupts_off();                                         \
    *(volatile T *)p = value;                                                  \
    interrupts_restore(flags);                                                 \
  }                                                                            \
  T exchange_##N(volatile void *p, T value, int order)                         \
      SYMBOL(__atomic_exchange_##N);                                           \
  T exchange_##N(volatile void *p, T value, int order) {                       \
    (void)order;                                                               \
    volatile T *q = (volatile T *)p;                                           \
    uint16_t flags = interrupts_off();                                         \
    T old = *q;                                                                \
    *q = value;                                                                \
    interrupts_restore(flags);                                                 \
    return old;                                                                \
  }                                                                            \
  bool compare_exchange_##N(volatile void *p, void *expected, T desired,       \
                            int success, int failure)                          \
      SYMBOL(__atomic_compare_exchange_##N);                                   \
  bool compare_exchange_##N(volatile void *p, void *expected, T desired,       \
                            int success, int failure) {                        \
    (void)success;                                                             \
    (void)failure;                                                             \
    volatile T *q = (volatile T *)p;                                           \
    T *e = (T *)expected;                                                      \
    uint16_t flags = interrupts_off();                                         \
    T old = *q;                                                                \
    bool equal = old == *e;                                                    \
    if (equal)                                                                 \
      *q = desired;                                                            \
    interrupts_restore(flags);                                                 \
    if (!equal)                                                                \
      *e = old;                                                                \
    return equal;                                                              \
  }                                                                            \
  FETCH(N, T, add, old + value)                                                \
  FETCH(N, T, sub, old - value)                                                \
  FETCH(N, T, and, old & value)                                                \
  FETCH(N, T, or, old | value)                                                 \
  FETCH(N, T, xor, old ^ value)                                                \
  FETCH(N, T, nand, ~(old & value))

SIZED(1, uint8_t)
SIZED(2, uint16_t)
SIZED(4, uint32_t)
SIZED(8, uint64_t)

// Objects of any other size.

void load(size_t size, const volatile void *src, void *dst, int order)
    SYMBOL(__atomic_load);
void load(size_t size, const volatile void *src, void *dst, int order) {
  (void)order;
  uint16_t flags = interrupts_off();
  copy(dst, src, size);
  interrupts_restore(flags);
}

void store(size_t size, volatile void *dst, void *src, int order)
    SYMBOL(__atomic_store);
void store(size_t size, volatile void *dst, void *src, int order) {
  (void)order;
  uint16_t flags = interrupts_off();
  copy(dst, src, size);
  interrupts_restore(flags);
}

void exchange(size_t size, volatile void *ptr, void *value, void *old,
              int order) SYMBOL(__atomic_exchange);
void exchange(size_t size, volatile void *ptr, void *value, void *old,
              int order) {
  (void)order;
  uint16_t flags = interrupts_off();
  copy(old, ptr, size);
  copy(ptr, value, size);
  interrupts_restore(flags);
}

bool compare_exchange(size_t size, volatile void *ptr, void *expected,
                      void *desired, int success, int failure)
    SYMBOL(__atomic_compare_exchange);
bool compare_exchange(size_t size, volatile void *ptr, void *expected,
                      void *desired, int success, int failure) {
  (void)success;
  (void)failure;
  volatile char *p = ptr;
  char *e = expected;
  size_t i;
  uint16_t flags = interrupts_off();
  for (i = 0; i < size && p[i] == e[i]; i++)
    ;
  bool equal = i == size;
  if (equal)
    copy(p, desired, size);
  else
    copy(e, p, size);
  interrupts_restore(flags);
  return equal;
}

// Turning interrupts off takes no lock, so every size is lock-free.
bool is_lock_free(size_t size, const volatile void *ptr)
    SYMBOL(__atomic_is_lock_free);
bool is_lock_free(size_t size, const volatile void *ptr) {
  (void)size;
  (void)ptr;
  return true;
}
