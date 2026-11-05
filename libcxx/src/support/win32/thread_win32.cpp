//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <__thread/support/windows.h>
#include <chrono>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <process.h>
#include <stdlib.h>
#include <windows.h>
#if _WIN32_WINNT >= 0x0600
#  include <fibersapi.h>
#endif

_LIBCPP_BEGIN_NAMESPACE_STD
_LIBCPP_BEGIN_EXPLICIT_ABI_ANNOTATIONS

// Windows 2000 and XP have no SRW locks, condition variables, one-time
// initialization, fiber local storage or GetThreadId: they came with Vista
// and Server 2003. With headers for an older Windows (_WIN32_WINNT below
// 0x0600), the same types hold a pointer to a CRITICAL_SECTION or a
// condition variable made on first use instead.
#if _WIN32_WINNT >= 0x0600
#  define _LIBCPP_WIN32_VISTA_SYNC 1
#else
#  define _LIBCPP_WIN32_VISTA_SYNC 0
#endif

// Windows 95, 98 and ME have no TryEnterCriticalSection. With headers for
// them and NT 4.0 (_WIN32_WINNT below 0x0500), a mutex is a count of the
// threads that want it and a semaphore they wait on, which can be tried
// everywhere, instead of a CRITICAL_SECTION.
#if _WIN32_WINNT < 0x0500
#  define _LIBCPP_WIN32_COUNT_MUTEX 1
#else
#  define _LIBCPP_WIN32_COUNT_MUTEX 0
#endif

#if _LIBCPP_WIN32_VISTA_SYNC
static_assert(sizeof(__libcpp_mutex_t) == sizeof(SRWLOCK), "");
static_assert(alignof(__libcpp_mutex_t) == alignof(SRWLOCK), "");
#endif

static_assert(sizeof(__libcpp_recursive_mutex_t) == sizeof(CRITICAL_SECTION), "");
static_assert(alignof(__libcpp_recursive_mutex_t) == alignof(CRITICAL_SECTION), "");

#if _LIBCPP_WIN32_VISTA_SYNC
static_assert(sizeof(__libcpp_condvar_t) == sizeof(CONDITION_VARIABLE), "");
static_assert(alignof(__libcpp_condvar_t) == alignof(CONDITION_VARIABLE), "");

static_assert(sizeof(__libcpp_exec_once_flag) == sizeof(INIT_ONCE), "");
static_assert(alignof(__libcpp_exec_once_flag) == alignof(INIT_ONCE), "");
#endif

static_assert(sizeof(__libcpp_thread_id) == sizeof(DWORD), "");
static_assert(alignof(__libcpp_thread_id) == alignof(DWORD), "");

static_assert(sizeof(__libcpp_thread_t) == sizeof(HANDLE), "");
static_assert(alignof(__libcpp_thread_t) == alignof(HANDLE), "");

static_assert(sizeof(__libcpp_tls_key) == sizeof(DWORD), "");
static_assert(alignof(__libcpp_tls_key) == alignof(DWORD), "");

#if _LIBCPP_WIN32_COUNT_MUTEX
namespace {
// Taking the mutex adds one to __count; a thread that finds it above zero
// waits for the semaphore, which the owner releases when it leaves with
// others still counted.
struct __count_mutex {
  LONG __count;
  HANDLE __sem;
};

bool __count_mutex_init(__count_mutex* __m) {
  __m->__count = 0;
  __m->__sem   = CreateSemaphoreA(nullptr, 0, 0x7fffffff, nullptr);
  return __m->__sem != nullptr;
}

void __count_mutex_lock(__count_mutex* __m) {
  if (__atomic_fetch_add(&__m->__count, 1, __ATOMIC_ACQ_REL) > 0)
    WaitForSingleObject(__m->__sem, INFINITE);
}

bool __count_mutex_trylock(__count_mutex* __m) {
  LONG __expected = 0;
  return __atomic_compare_exchange_n(&__m->__count, &__expected, 1, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
}

void __count_mutex_unlock(__count_mutex* __m) {
  if (__atomic_fetch_sub(&__m->__count, 1, __ATOMIC_ACQ_REL) > 1)
    ReleaseSemaphore(__m->__sem, 1, nullptr);
}

// A recursive mutex is one of those, its owner and its depth, in place.
struct __count_recursive_mutex {
  __count_mutex __m;
  DWORD __owner;
  LONG __depth;
};
static_assert(sizeof(__count_recursive_mutex) <= sizeof(__libcpp_recursive_mutex_t), "");

__count_recursive_mutex* __recursive(__libcpp_recursive_mutex_t* __m) {
  return reinterpret_cast<__count_recursive_mutex*>(__m);
}

bool __owns(__count_recursive_mutex* __r) {
  return __atomic_load_n(&__r->__owner, __ATOMIC_RELAXED) == GetCurrentThreadId();
}

void __take(__count_recursive_mutex* __r) {
  __atomic_store_n(&__r->__owner, GetCurrentThreadId(), __ATOMIC_RELAXED);
  __r->__depth = 1;
}
} // namespace

// Mutex
int __libcpp_recursive_mutex_init(__libcpp_recursive_mutex_t* __m) {
  __count_recursive_mutex* __r = __recursive(__m);
  __r->__owner                 = 0;
  __r->__depth                 = 0;
  return __count_mutex_init(&__r->__m) ? 0 : GetLastError();
}

int __libcpp_recursive_mutex_lock(__libcpp_recursive_mutex_t* __m) {
  __count_recursive_mutex* __r = __recursive(__m);
  if (__owns(__r)) {
    ++__r->__depth;
    return 0;
  }
  __count_mutex_lock(&__r->__m);
  __take(__r);
  return 0;
}

bool __libcpp_recursive_mutex_trylock(__libcpp_recursive_mutex_t* __m) {
  __count_recursive_mutex* __r = __recursive(__m);
  if (__owns(__r)) {
    ++__r->__depth;
    return true;
  }
  if (!__count_mutex_trylock(&__r->__m))
    return false;
  __take(__r);
  return true;
}

int __libcpp_recursive_mutex_unlock(__libcpp_recursive_mutex_t* __m) {
  __count_recursive_mutex* __r = __recursive(__m);
  if (--__r->__depth)
    return 0;
  __atomic_store_n(&__r->__owner, 0, __ATOMIC_RELAXED);
  __count_mutex_unlock(&__r->__m);
  return 0;
}

int __libcpp_recursive_mutex_destroy(__libcpp_recursive_mutex_t* __m) {
  CloseHandle(__recursive(__m)->__m.__sem);
  return 0;
}
#else  // _LIBCPP_WIN32_COUNT_MUTEX

// Mutex
int __libcpp_recursive_mutex_init(__libcpp_recursive_mutex_t* __m) {
  InitializeCriticalSection((LPCRITICAL_SECTION)__m);
  return 0;
}

int __libcpp_recursive_mutex_lock(__libcpp_recursive_mutex_t* __m) {
  EnterCriticalSection((LPCRITICAL_SECTION)__m);
  return 0;
}

bool __libcpp_recursive_mutex_trylock(__libcpp_recursive_mutex_t* __m) {
  return TryEnterCriticalSection((LPCRITICAL_SECTION)__m) != 0;
}

int __libcpp_recursive_mutex_unlock(__libcpp_recursive_mutex_t* __m) {
  LeaveCriticalSection((LPCRITICAL_SECTION)__m);
  return 0;
}

int __libcpp_recursive_mutex_destroy(__libcpp_recursive_mutex_t* __m) {
  DeleteCriticalSection((LPCRITICAL_SECTION)__m);
  return 0;
}
#endif // _LIBCPP_WIN32_COUNT_MUTEX

#if _LIBCPP_WIN32_VISTA_SYNC
int __libcpp_mutex_lock(__libcpp_mutex_t* __m) {
  AcquireSRWLockExclusive((PSRWLOCK)__m);
  return 0;
}

bool __libcpp_mutex_trylock(__libcpp_mutex_t* __m) { return TryAcquireSRWLockExclusive((PSRWLOCK)__m) != 0; }

int __libcpp_mutex_unlock(__libcpp_mutex_t* __m) {
  ReleaseSRWLockExclusive((PSRWLOCK)__m);
  return 0;
}

int __libcpp_mutex_destroy(__libcpp_mutex_t* __m) {
  static_cast<void>(__m);
  return 0;
}

// Condition Variable
int __libcpp_condvar_signal(__libcpp_condvar_t* __cv) {
  WakeConditionVariable((PCONDITION_VARIABLE)__cv);
  return 0;
}

int __libcpp_condvar_broadcast(__libcpp_condvar_t* __cv) {
  WakeAllConditionVariable((PCONDITION_VARIABLE)__cv);
  return 0;
}

int __libcpp_condvar_wait(__libcpp_condvar_t* __cv, __libcpp_mutex_t* __m) {
  SleepConditionVariableSRW((PCONDITION_VARIABLE)__cv, (PSRWLOCK)__m, INFINITE, 0);
  return 0;
}

int __libcpp_condvar_timedwait(__libcpp_condvar_t* __cv, __libcpp_mutex_t* __m, __libcpp_timespec_t* __ts) {
  using namespace std::chrono;

  auto duration   = seconds(__ts->tv_sec) + nanoseconds(__ts->tv_nsec);
  auto abstime    = system_clock::time_point(duration_cast<system_clock::duration>(duration));
  auto timeout_ms = duration_cast<milliseconds>(abstime - system_clock::now());

  if (!SleepConditionVariableSRW(
          (PCONDITION_VARIABLE)__cv, (PSRWLOCK)__m, timeout_ms.count() > 0 ? timeout_ms.count() : 0, 0)) {
    auto __ec = GetLastError();
    return __ec == ERROR_TIMEOUT ? ETIMEDOUT : __ec;
  }
  return 0;
}

int __libcpp_condvar_destroy(__libcpp_condvar_t* __cv) {
  static_cast<void>(__cv);
  return 0;
}

// Execute Once
static inline _LIBCPP_HIDE_FROM_ABI BOOL CALLBACK
__libcpp_init_once_execute_once_thunk(PINIT_ONCE __init_once, PVOID __parameter, PVOID* __context) {
  static_cast<void>(__init_once);
  static_cast<void>(__context);

  void (*init_routine)(void) = reinterpret_cast<void (*)(void)>(__parameter);
  init_routine();
  return TRUE;
}

int __libcpp_execute_once(__libcpp_exec_once_flag* __flag, void (*__init_routine)(void)) {
  if (!InitOnceExecuteOnce(
          (PINIT_ONCE)__flag, __libcpp_init_once_execute_once_thunk, reinterpret_cast<void*>(__init_routine), nullptr))
    return GetLastError();
  return 0;
}

#else // _LIBCPP_WIN32_VISTA_SYNC

namespace {

// The address in *__slot, or the one that __make stores there. Two threads
// can race to make it; the loser frees its copy.
template <class _Tp, class _Make, class _Free>
_Tp* __get_or_make(void** __slot, _Make __make, _Free __free) {
  if (void* __p = __atomic_load_n(__slot, __ATOMIC_ACQUIRE))
    return static_cast<_Tp*>(__p);
  _Tp* __new = __make();
  if (!__new)
    abort();
  void* __expected = nullptr;
  if (__atomic_compare_exchange_n(__slot, &__expected, __new, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    return __new;
  __free(__new);
  return static_cast<_Tp*>(__expected);
}

#  if !_LIBCPP_WIN32_COUNT_MUTEX
CRITICAL_SECTION* __new_critical_section() {
  auto* __cs = static_cast<CRITICAL_SECTION*>(malloc(sizeof(CRITICAL_SECTION)));
  if (__cs)
    InitializeCriticalSection(__cs);
  return __cs;
}

void __free_critical_section(CRITICAL_SECTION* __cs) {
  DeleteCriticalSection(__cs);
  free(__cs);
}
#  endif

#  if _LIBCPP_WIN32_COUNT_MUTEX
__count_mutex* __new_count_mutex() {
  auto* __cm = static_cast<__count_mutex*>(malloc(sizeof(__count_mutex)));
  if (__cm && !__count_mutex_init(__cm)) {
    free(__cm);
    __cm = nullptr;
  }
  return __cm;
}

void __free_count_mutex(__count_mutex* __cm) {
  CloseHandle(__cm->__sem);
  free(__cm);
}

__count_mutex* __mutex_cm(__libcpp_mutex_t* __m) {
  return __get_or_make<__count_mutex>(__m, __new_count_mutex, __free_count_mutex);
}

void __mutex_enter(__libcpp_mutex_t* __m) { __count_mutex_lock(__mutex_cm(__m)); }
bool __mutex_try(__libcpp_mutex_t* __m) { return __count_mutex_trylock(__mutex_cm(__m)); }
void __mutex_leave(__libcpp_mutex_t* __m) { __count_mutex_unlock(__mutex_cm(__m)); }
void __mutex_free(void* __p) { __free_count_mutex(static_cast<__count_mutex*>(__p)); }
#  else
CRITICAL_SECTION* __mutex_cs(__libcpp_mutex_t* __m) {
  return __get_or_make<CRITICAL_SECTION>(__m, __new_critical_section, __free_critical_section);
}

void __mutex_enter(__libcpp_mutex_t* __m) { EnterCriticalSection(__mutex_cs(__m)); }
bool __mutex_try(__libcpp_mutex_t* __m) { return TryEnterCriticalSection(__mutex_cs(__m)) != 0; }
void __mutex_leave(__libcpp_mutex_t* __m) { LeaveCriticalSection(__mutex_cs(__m)); }
void __mutex_free(void* __p) { __free_critical_section(static_cast<CRITICAL_SECTION*>(__p)); }
#  endif

// A condition variable is a queue of waiting threads, each with its own
// event, so a signal wakes exactly one thread that was waiting when it came.
struct __cv_waiter {
  HANDLE __event;
  bool __signaled;
  __cv_waiter* __next;
};

struct __cv_state {
  CRITICAL_SECTION __lock;
  __cv_waiter* __head;
  __cv_waiter* __tail;
};

__cv_state* __new_cv_state() {
  auto* __st = static_cast<__cv_state*>(malloc(sizeof(__cv_state)));
  if (__st) {
    InitializeCriticalSection(&__st->__lock);
    __st->__head = __st->__tail = nullptr;
  }
  return __st;
}

void __free_cv_state(__cv_state* __st) {
  DeleteCriticalSection(&__st->__lock);
  free(__st);
}

__cv_state* __condvar_state(__libcpp_condvar_t* __cv) {
  return __get_or_make<__cv_state>(__cv, __new_cv_state, __free_cv_state);
}

// Wakes the first waiter, or every waiter. The event is set with the lock
// held: the waiter closes it once it sees __signaled under the same lock.
void __condvar_wake(__libcpp_condvar_t* __cv, bool __all) {
  if (!__atomic_load_n(__cv, __ATOMIC_ACQUIRE))
    return; // nobody ever waited
  __cv_state* __st = __condvar_state(__cv);
  EnterCriticalSection(&__st->__lock);
  while (__cv_waiter* __w = __st->__head) {
    __st->__head = __w->__next;
    if (!__st->__head)
      __st->__tail = nullptr;
    __w->__signaled = true;
    SetEvent(__w->__event);
    if (!__all)
      break;
  }
  LeaveCriticalSection(&__st->__lock);
}

int __condvar_wait(__libcpp_condvar_t* __cv, __libcpp_mutex_t* __m, DWORD __ms) {
  __cv_state* __st = __condvar_state(__cv);
  __cv_waiter __w;
  __w.__event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
  if (!__w.__event)
    return GetLastError();
  __w.__signaled = false;
  __w.__next     = nullptr;

  EnterCriticalSection(&__st->__lock);
  if (__st->__tail)
    __st->__tail->__next = &__w;
  else
    __st->__head = &__w;
  __st->__tail = &__w;
  LeaveCriticalSection(&__st->__lock);

  __mutex_leave(__m);
  WaitForSingleObject(__w.__event, __ms);

  // Signaled or not, take the waiter off the queue before it goes away.
  EnterCriticalSection(&__st->__lock);
  bool __timed_out = !__w.__signaled;
  if (__timed_out) {
    __cv_waiter* __prev = nullptr;
    for (__cv_waiter* __i = __st->__head; __i; __prev = __i, __i = __i->__next) {
      if (__i == &__w) {
        (__prev ? __prev->__next : __st->__head) = __w.__next;
        if (__st->__tail == &__w)
          __st->__tail = __prev;
        break;
      }
    }
  }
  LeaveCriticalSection(&__st->__lock);
  CloseHandle(__w.__event);

  __mutex_enter(__m);
  return __timed_out ? ETIMEDOUT : 0;
}

} // namespace

int __libcpp_mutex_lock(__libcpp_mutex_t* __m) {
  __mutex_enter(__m);
  return 0;
}

bool __libcpp_mutex_trylock(__libcpp_mutex_t* __m) { return __mutex_try(__m); }

int __libcpp_mutex_unlock(__libcpp_mutex_t* __m) {
  __mutex_leave(__m);
  return 0;
}

int __libcpp_mutex_destroy(__libcpp_mutex_t* __m) {
  if (*__m)
    __mutex_free(*__m);
  *__m = nullptr;
  return 0;
}

// Condition Variable
int __libcpp_condvar_signal(__libcpp_condvar_t* __cv) {
  __condvar_wake(__cv, false);
  return 0;
}

int __libcpp_condvar_broadcast(__libcpp_condvar_t* __cv) {
  __condvar_wake(__cv, true);
  return 0;
}

int __libcpp_condvar_wait(__libcpp_condvar_t* __cv, __libcpp_mutex_t* __m) {
  return __condvar_wait(__cv, __m, INFINITE);
}

int __libcpp_condvar_timedwait(__libcpp_condvar_t* __cv, __libcpp_mutex_t* __m, __libcpp_timespec_t* __ts) {
  using namespace std::chrono;

  auto duration   = seconds(__ts->tv_sec) + nanoseconds(__ts->tv_nsec);
  auto abstime    = system_clock::time_point(duration_cast<system_clock::duration>(duration));
  auto timeout_ms = duration_cast<milliseconds>(abstime - system_clock::now());
  auto __ms       = timeout_ms.count() > 0 ? timeout_ms.count() : 0;
  return __condvar_wait(__cv, __m, __ms < INFINITE ? static_cast<DWORD>(__ms) : INFINITE - 1);
}

int __libcpp_condvar_destroy(__libcpp_condvar_t* __cv) {
  if (*__cv)
    __free_cv_state(static_cast<__cv_state*>(*__cv));
  *__cv = nullptr;
  return 0;
}

// Execute Once: the flag goes from 0 to 1 while the routine runs, then to 2.
int __libcpp_execute_once(__libcpp_exec_once_flag* __flag, void (*__init_routine)(void)) {
  auto* __state = reinterpret_cast<uintptr_t*>(__flag);
  if (__atomic_load_n(__state, __ATOMIC_ACQUIRE) == 2)
    return 0;
  uintptr_t __expected = 0;
  if (__atomic_compare_exchange_n(__state, &__expected, 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    __init_routine();
    __atomic_store_n(__state, 2, __ATOMIC_RELEASE);
    return 0;
  }
  for (unsigned __spins = 0; __atomic_load_n(__state, __ATOMIC_ACQUIRE) != 2; ++__spins)
    Sleep(__spins < 16 ? 0 : 1);
  return 0;
}

#endif // _LIBCPP_WIN32_VISTA_SYNC

// Thread ID
bool __libcpp_thread_id_equal(__libcpp_thread_id __lhs, __libcpp_thread_id __rhs) { return __lhs == __rhs; }

bool __libcpp_thread_id_less(__libcpp_thread_id __lhs, __libcpp_thread_id __rhs) { return __lhs < __rhs; }

// Thread
struct __libcpp_beginthreadex_thunk_data {
  void* (*__func)(void*);
  void* __arg;
};

static inline _LIBCPP_HIDE_FROM_ABI unsigned WINAPI __libcpp_beginthreadex_thunk(void* __raw_data) {
  auto* __data = static_cast<__libcpp_beginthreadex_thunk_data*>(__raw_data);
  auto* __func = __data->__func;
  void* __arg  = __data->__arg;
  delete __data;
  return static_cast<unsigned>(reinterpret_cast<uintptr_t>(__func(__arg)));
}

bool __libcpp_thread_isnull(const __libcpp_thread_t* __t) { return *__t == 0; }

#if !_LIBCPP_WIN32_VISTA_SYNC
namespace {
using __get_thread_id_t = DWORD(WINAPI*)(HANDLE);
using __query_thread_t  = LONG(WINAPI*)(HANDLE, int, PVOID, ULONG, PULONG);

FARPROC __system_function(const char* __dll, const char* __name) {
  HMODULE __module = GetModuleHandleA(__dll);
  return __module ? GetProcAddress(__module, __name) : nullptr;
}

// GetThreadId is from Server 2003 and Vista. Before that, ntdll's
// NtQueryInformationThread gives the thread's client ID.
__get_thread_id_t __get_thread_id() {
  static auto __f = reinterpret_cast<__get_thread_id_t>(__system_function("kernel32.dll", "GetThreadId"));
  return __f;
}

__query_thread_t __query_thread() {
  static auto __f = reinterpret_cast<__query_thread_t>(__system_function("ntdll.dll", "NtQueryInformationThread"));
  return __f;
}

// Windows 95, 98 and ME have neither, and no other way from a thread's
// handle to its ID. There, the threads made here are kept in a list with
// their IDs until they are joined or detached.
struct __thread_entry {
  HANDLE __handle;
  DWORD __id;
  __thread_entry* __next;
};
__thread_entry* __threads;
LONG __threads_lock;

bool __keeps_thread_ids() { return !__get_thread_id() && !__query_thread(); }

void __lock_threads() {
  while (__atomic_exchange_n(&__threads_lock, 1, __ATOMIC_ACQUIRE))
    Sleep(0);
}

void __unlock_threads() { __atomic_store_n(&__threads_lock, 0, __ATOMIC_RELEASE); }

void __remember_thread(HANDLE __h, DWORD __id) {
  auto* __e = static_cast<__thread_entry*>(malloc(sizeof(__thread_entry)));
  if (!__e)
    return;
  __e->__handle = __h;
  __e->__id     = __id;
  __lock_threads();
  __e->__next = __threads;
  __threads   = __e;
  __unlock_threads();
}

DWORD __remembered_thread_id(HANDLE __h, bool __forget) {
  DWORD __id = 0;
  __lock_threads();
  for (__thread_entry** __p = &__threads; *__p; __p = &(*__p)->__next) {
    if ((*__p)->__handle == __h) {
      __thread_entry* __e = *__p;
      __id                = __e->__id;
      if (__forget) {
        *__p = __e->__next;
        free(__e);
      }
      break;
    }
  }
  __unlock_threads();
  return __id;
}
} // namespace
#endif

int __libcpp_thread_create(__libcpp_thread_t* __t, void* (*__func)(void*), void* __arg) {
  auto* __data   = new __libcpp_beginthreadex_thunk_data;
  __data->__func = __func;
  __data->__arg  = __arg;

  unsigned __id = 0;
  *__t          = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, __libcpp_beginthreadex_thunk, __data, 0, &__id));

  if (*__t) {
#if !_LIBCPP_WIN32_VISTA_SYNC
    if (__keeps_thread_ids())
      __remember_thread(*__t, __id);
#endif
    return 0;
  }
  return GetLastError();
}

__libcpp_thread_id __libcpp_thread_get_current_id() { return GetCurrentThreadId(); }

#if _LIBCPP_WIN32_VISTA_SYNC
__libcpp_thread_id __libcpp_thread_get_id(const __libcpp_thread_t* __t) { return GetThreadId(*__t); }
#else
__libcpp_thread_id __libcpp_thread_get_id(const __libcpp_thread_t* __t) {
  if (auto __f = __get_thread_id())
    return __f(*__t);

  struct {
    LONG __exit_status;
    PVOID __teb;
    HANDLE __process_id;
    HANDLE __thread_id;
    ULONG_PTR __affinity;
    LONG __priority;
    LONG __base_priority;
  } __info;
  if (auto __query = __query_thread()) {
    if (__query(*__t, 0 /* ThreadBasicInformation */, &__info, sizeof(__info), nullptr) >= 0)
      return static_cast<__libcpp_thread_id>(reinterpret_cast<uintptr_t>(__info.__thread_id));
    return 0;
  }
  return __remembered_thread_id(*__t, false);
}
#endif

int __libcpp_thread_join(__libcpp_thread_t* __t) {
  if (WaitForSingleObjectEx(*__t, INFINITE, FALSE) == WAIT_FAILED)
    return GetLastError();
#if !_LIBCPP_WIN32_VISTA_SYNC
  if (__keeps_thread_ids())
    __remembered_thread_id(*__t, true);
#endif
  if (!CloseHandle(*__t))
    return GetLastError();
  return 0;
}

int __libcpp_thread_detach(__libcpp_thread_t* __t) {
#if !_LIBCPP_WIN32_VISTA_SYNC
  if (__keeps_thread_ids())
    __remembered_thread_id(*__t, true);
#endif
  if (!CloseHandle(*__t))
    return GetLastError();
  return 0;
}

#if _WIN32_WINNT < 0x0500
// SwitchToThread is from NT 4.0; Windows 95, 98 and ME have only Sleep.
void __libcpp_thread_yield() { Sleep(0); }
#else
void __libcpp_thread_yield() { SwitchToThread(); }
#endif

void __libcpp_thread_sleep_for(const chrono::nanoseconds& __ns) {
  // round-up to the nearest millisecond
  chrono::milliseconds __ms = chrono::ceil<chrono::milliseconds>(__ns);
  // FIXME(compnerd) this should be an alertable sleep (WFSO or SleepEx)
  Sleep(__ms.count());
}

#if _LIBCPP_WIN32_VISTA_SYNC
// Thread Local Storage
int __libcpp_tls_create(__libcpp_tls_key* __key, void(_LIBCPP_TLS_DESTRUCTOR_CC* __at_exit)(void*)) {
  DWORD index = FlsAlloc(__at_exit);
  if (index == FLS_OUT_OF_INDEXES)
    return GetLastError();
  *__key = index;
  return 0;
}

void* __libcpp_tls_get(__libcpp_tls_key __key) { return FlsGetValue(__key); }

int __libcpp_tls_set(__libcpp_tls_key __key, void* __p) {
  if (!FlsSetValue(__key, __p))
    return GetLastError();
  return 0;
}

#else // _LIBCPP_WIN32_VISTA_SYNC

// Thread Local Storage. Fiber local storage, which runs a destructor when a
// thread exits, is from Server 2003 and Vista. Before that, the destructors
// run from the mingw-w64 runtime's thread exit callback: one callback walks
// the keys made here, for each thread that set a value.
extern "C" int ___w64_mingwthr_add_key_dtor(DWORD, void (*)(void*));

namespace {
struct __tls_dtor {
  DWORD __key;
  void(_LIBCPP_TLS_DESTRUCTOR_CC* __dtor)(void*);
  __tls_dtor* __next;
};

__tls_dtor* __tls_dtors;
DWORD __tls_marker                        = TLS_OUT_OF_INDEXES; // set in threads that hold a value
__libcpp_exec_once_flag __tls_marker_once = _LIBCPP_EXEC_ONCE_INITIALIZER;

void __run_tls_dtors(void*) {
  // A destructor can set values again, as with pthread keys.
  for (int __round = 0; __round < 4; ++__round) {
    bool __ran = false;
    for (__tls_dtor* __d = __atomic_load_n(&__tls_dtors, __ATOMIC_ACQUIRE); __d; __d = __d->__next) {
      if (void* __p = TlsGetValue(__d->__key)) {
        TlsSetValue(__d->__key, nullptr);
        __d->__dtor(__p);
        __ran = true;
      }
    }
    if (!__ran)
      break;
  }
}

void __make_tls_marker() {
  DWORD __key = TlsAlloc();
  if (__key != TLS_OUT_OF_INDEXES && ___w64_mingwthr_add_key_dtor(__key, __run_tls_dtors) == 0)
    __tls_marker = __key;
}
} // namespace

int __libcpp_tls_create(__libcpp_tls_key* __key, void(_LIBCPP_TLS_DESTRUCTOR_CC* __at_exit)(void*)) {
  DWORD index = TlsAlloc();
  if (index == TLS_OUT_OF_INDEXES)
    return GetLastError();
  if (__at_exit) {
    __libcpp_execute_once(&__tls_marker_once, __make_tls_marker);
    auto* __d = static_cast<__tls_dtor*>(malloc(sizeof(__tls_dtor)));
    if (!__d) {
      TlsFree(index);
      return ERROR_NOT_ENOUGH_MEMORY;
    }
    __d->__key  = index;
    __d->__dtor = __at_exit;
    __d->__next = __atomic_load_n(&__tls_dtors, __ATOMIC_RELAXED);
    while (!__atomic_compare_exchange_n(&__tls_dtors, &__d->__next, __d, true, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
    }
  }
  *__key = index;
  return 0;
}

void* __libcpp_tls_get(__libcpp_tls_key __key) { return TlsGetValue(__key); }

int __libcpp_tls_set(__libcpp_tls_key __key, void* __p) {
  if (!TlsSetValue(__key, __p))
    return GetLastError();
  if (__p && __tls_marker != TLS_OUT_OF_INDEXES)
    TlsSetValue(__tls_marker, reinterpret_cast<void*>(1));
  return 0;
}

#endif // _LIBCPP_WIN32_VISTA_SYNC

_LIBCPP_END_EXPLICIT_ABI_ANNOTATIONS
_LIBCPP_END_NAMESPACE_STD
