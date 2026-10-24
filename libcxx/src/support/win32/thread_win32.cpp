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

CRITICAL_SECTION* __mutex_cs(__libcpp_mutex_t* __m) {
  return __get_or_make<CRITICAL_SECTION>(__m, __new_critical_section, __free_critical_section);
}

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
  __w.__event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
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

  LeaveCriticalSection(__mutex_cs(__m));
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

  EnterCriticalSection(__mutex_cs(__m));
  return __timed_out ? ETIMEDOUT : 0;
}

} // namespace

int __libcpp_mutex_lock(__libcpp_mutex_t* __m) {
  EnterCriticalSection(__mutex_cs(__m));
  return 0;
}

bool __libcpp_mutex_trylock(__libcpp_mutex_t* __m) { return TryEnterCriticalSection(__mutex_cs(__m)) != 0; }

int __libcpp_mutex_unlock(__libcpp_mutex_t* __m) {
  LeaveCriticalSection(__mutex_cs(__m));
  return 0;
}

int __libcpp_mutex_destroy(__libcpp_mutex_t* __m) {
  if (*__m)
    __free_critical_section(static_cast<CRITICAL_SECTION*>(*__m));
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

int __libcpp_thread_create(__libcpp_thread_t* __t, void* (*__func)(void*), void* __arg) {
  auto* __data   = new __libcpp_beginthreadex_thunk_data;
  __data->__func = __func;
  __data->__arg  = __arg;

  *__t = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, __libcpp_beginthreadex_thunk, __data, 0, nullptr));

  if (*__t)
    return 0;
  return GetLastError();
}

__libcpp_thread_id __libcpp_thread_get_current_id() { return GetCurrentThreadId(); }

#if _LIBCPP_WIN32_VISTA_SYNC
__libcpp_thread_id __libcpp_thread_get_id(const __libcpp_thread_t* __t) { return GetThreadId(*__t); }
#else
// GetThreadId is from Server 2003 and Vista. Before that, ntdll's
// NtQueryInformationThread gives the thread's client ID.
__libcpp_thread_id __libcpp_thread_get_id(const __libcpp_thread_t* __t) {
  using __get_thread_id_t = DWORD(WINAPI*)(HANDLE);
  static auto __get_thread_id =
      reinterpret_cast<__get_thread_id_t>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadId"));
  if (__get_thread_id)
    return __get_thread_id(*__t);

  struct {
    LONG __exit_status;
    PVOID __teb;
    HANDLE __process_id;
    HANDLE __thread_id;
    ULONG_PTR __affinity;
    LONG __priority;
    LONG __base_priority;
  } __info;
  using __query_t = LONG(WINAPI*)(HANDLE, int, PVOID, ULONG, PULONG);
  static auto __query =
      reinterpret_cast<__query_t>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread"));
  if (__query && __query(*__t, 0 /* ThreadBasicInformation */, &__info, sizeof(__info), nullptr) >= 0)
    return static_cast<__libcpp_thread_id>(reinterpret_cast<uintptr_t>(__info.__thread_id));
  return 0;
}
#endif

int __libcpp_thread_join(__libcpp_thread_t* __t) {
  if (WaitForSingleObjectEx(*__t, INFINITE, FALSE) == WAIT_FAILED)
    return GetLastError();
  if (!CloseHandle(*__t))
    return GetLastError();
  return 0;
}

int __libcpp_thread_detach(__libcpp_thread_t* __t) {
  if (!CloseHandle(*__t))
    return GetLastError();
  return 0;
}

void __libcpp_thread_yield() { SwitchToThread(); }

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
