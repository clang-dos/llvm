//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <__config>
#include <__verbose_abort>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#ifdef __BIONIC__
#  include <syslog.h>
extern "C" void android_set_abort_message(const char* msg);
#endif   // __BIONIC__

#if defined(__APPLE__) && __has_include(<CrashReporterClient.h>)
#  include <CrashReporterClient.h>
#endif

_LIBCPP_BEGIN_NAMESPACE_STD
_LIBCPP_BEGIN_EXPLICIT_ABI_ANNOTATIONS

#if defined(__I86__)
// The 8086 formats these messages itself. vfprintf would link the whole
// floating-point printf (about 13 KB of 8086 code) into every program that
// can abort, and the messages only use %s, %c and the integer conversions.
static void __i86_print_unsigned(unsigned long __v, unsigned __base, bool __neg) {
  char __buf[12];
  char* __p = __buf + sizeof(__buf);
  *--__p = '\0';
  do {
    *--__p = "0123456789abcdef"[__v % __base];
    __v /= __base;
  } while (__v);
  if (__neg)
    *--__p = '-';
  fputs(__p, stderr);
}

static void __i86_vprint(const char* __fmt, va_list __ap) {
  for (; *__fmt; ++__fmt) {
    if (*__fmt != '%') {
      fputc(*__fmt, stderr);
      continue;
    }
    const char* __spec = __fmt++;
    bool __long        = false;
    for (; *__fmt == 'l' || *__fmt == 'z'; ++__fmt)
      __long = *__fmt == 'l' || sizeof(std::size_t) == sizeof(long);
    switch (*__fmt) {
    case 's': {
      const char* __s = va_arg(__ap, const char*);
      fputs(__s ? __s : "(null)", stderr);
      break;
    }
    case 'c':
      fputc(va_arg(__ap, int), stderr);
      break;
    case 'd':
    case 'i': {
      long __v = __long ? va_arg(__ap, long) : va_arg(__ap, int);
      __i86_print_unsigned(__v < 0 ? 0UL - (unsigned long)__v : (unsigned long)__v, 10, __v < 0);
      break;
    }
    case 'u':
    case 'x': {
      unsigned long __v = __long ? va_arg(__ap, unsigned long) : va_arg(__ap, unsigned);
      __i86_print_unsigned(__v, *__fmt == 'x' ? 16 : 10, false);
      break;
    }
    case '%':
      fputc('%', stderr);
      break;
    default: // Not used by these messages: print the conversion as written.
      for (; __spec <= __fmt && *__spec; ++__spec)
        fputc(*__spec, stderr);
      if (!*__fmt)
        return;
    }
  }
}
#endif

[[gnu::weak]] void __libcpp_verbose_abort(char const* format, ...) noexcept {
  // Write message to stderr. We do this before formatting into a
  // buffer so that we still get some information out if that fails.
  {
    va_list list;
    va_start(list, format);
#if defined(__I86__)
    __i86_vprint(format, list);
#else
    std::vfprintf(stderr, format, list);
#endif
    va_end(list);
  }

  // Format the arguments into an allocated buffer for CrashReport & friends.
  // We leak the buffer on purpose, since we're about to abort() anyway.
  char* buffer;
  (void)buffer;
  va_list list;
  va_start(list, format);

#if defined(__APPLE__) && __has_include(<CrashReporterClient.h>)
  // Note that we should technically synchronize accesses here (by e.g. taking a lock),
  // however concretely we're only setting a pointer, so the likelihood of a race here
  // is low.
  vasprintf(&buffer, format, list);
  CRSetCrashLogMessage(buffer);
#elif defined(__BIONIC__)
  vasprintf(&buffer, format, list);

  // Show error in tombstone.
  android_set_abort_message(buffer);

  // Show error in logcat.
  openlog("libc++", 0, 0);
  syslog(LOG_CRIT, "%s", buffer);
  closelog();
#endif
  va_end(list);

  std::abort();
}

_LIBCPP_END_EXPLICIT_ABI_ANNOTATIONS
_LIBCPP_END_NAMESPACE_STD
