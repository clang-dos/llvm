//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//
// Abstract interface to shared reader/writer log, hiding platform and
// configuration differences.
//
//===----------------------------------------------------------------------===//

#ifndef __RWMUTEX_HPP__
#define __RWMUTEX_HPP__

#if defined(_WIN32)
#include <windows.h>
#elif !defined(_LIBUNWIND_HAS_NO_THREADS)
#include <pthread.h>
#if defined(__ELF__) && defined(_LIBUNWIND_LINK_PTHREAD_LIB)
#pragma comment(lib, "pthread")
#endif
#endif

namespace libunwind {

#if defined(_LIBUNWIND_HAS_NO_THREADS)

class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() { return true; }
  bool unlock_shared() { return true; }
  bool lock() { return true; }
  bool unlock() { return true; }
};

#elif defined(_WIN32) && defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600

// Windows 2000 and XP have no SRW locks. The lock is held only briefly, and
// a CRITICAL_SECTION would need a constructor for the global locks, so this
// is a spin lock, shared or not.
class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() { return lock(); }
  bool unlock_shared() { return unlock(); }
  bool lock() {
    for (unsigned spins = 0; InterlockedExchange(&_lock, 1) != 0; ++spins)
      Sleep(spins < 16 ? 0 : 1); // 1 lets a lower-priority owner run
    return true;
  }
  bool unlock() {
    InterlockedExchange(&_lock, 0);
    return true;
  }

private:
  LONG volatile _lock = 0;
};

#elif defined(_WIN32)

class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() {
    AcquireSRWLockShared(&_lock);
    return true;
  }
  bool unlock_shared() {
    ReleaseSRWLockShared(&_lock);
    return true;
  }
  bool lock() {
    AcquireSRWLockExclusive(&_lock);
    return true;
  }
  bool unlock() {
    ReleaseSRWLockExclusive(&_lock);
    return true;
  }

private:
  SRWLOCK _lock = SRWLOCK_INIT;
};

#elif !defined(LIBUNWIND_USE_WEAK_PTHREAD)

class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() { return pthread_rwlock_rdlock(&_lock) == 0;  }
  bool unlock_shared() { return pthread_rwlock_unlock(&_lock) == 0; }
  bool lock() { return pthread_rwlock_wrlock(&_lock) == 0; }
  bool unlock() { return pthread_rwlock_unlock(&_lock) == 0; }

private:
  pthread_rwlock_t _lock = PTHREAD_RWLOCK_INITIALIZER;
};

#else

extern "C" int __attribute__((weak))
pthread_create(pthread_t *thread, const pthread_attr_t *attr,
               void *(*start_routine)(void *), void *arg);
extern "C" int __attribute__((weak))
pthread_rwlock_rdlock(pthread_rwlock_t *lock);
extern "C" int __attribute__((weak))
pthread_rwlock_wrlock(pthread_rwlock_t *lock);
extern "C" int __attribute__((weak))
pthread_rwlock_unlock(pthread_rwlock_t *lock);

// Calls to the locking functions are gated on pthread_create, and not the
// functions themselves, because the data structure should only be locked if
// another thread has been created. This is what similar libraries do.

class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() {
    return !pthread_create || (pthread_rwlock_rdlock(&_lock) == 0);
  }
  bool unlock_shared() {
    return !pthread_create || (pthread_rwlock_unlock(&_lock) == 0);
  }
  bool lock() {
    return !pthread_create || (pthread_rwlock_wrlock(&_lock) == 0);
  }
  bool unlock() {
    return !pthread_create || (pthread_rwlock_unlock(&_lock) == 0);
  }

private:
  pthread_rwlock_t _lock = PTHREAD_RWLOCK_INITIALIZER;
};

#endif

} // namespace libunwind

#endif // __RWMUTEX_HPP__
