//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include "abort_message.h"

#if defined(__I86__)
// The 8086 formats these messages itself. vfprintf would link the whole
// floating-point printf (about 13 KB of 8086 code) into every program that
// can abort. The messages use only %s and %d or %i with an int; anything
// else is printed as written.
static void __i86_vprint(const char* __fmt, va_list __ap) {
  for (;;) {
    const char* __p = __fmt;
    while (*__p && *__p != '%')
      ++__p;
    fwrite(__fmt, 1, __p - __fmt, stderr);
    if (!*__p)
      return;
    char __c = __p[1];
    if (__c == 's') {
      const char* __s = va_arg(__ap, const char*);
      fputs(__s ? __s : "(null)", stderr);
    } else if (__c == 'd' || __c == 'i') {
      int __v = va_arg(__ap, int);
      unsigned __u = __v < 0 ? 0U - (unsigned)__v : (unsigned)__v;
      char __buf[8];
      char* __d = __buf + sizeof(__buf);
      *--__d = '\0';
      do {
        *--__d = '0' + __u % 10;
        __u /= 10;
      } while (__u);
      if (__v < 0)
        *--__d = '-';
      fputs(__d, stderr);
    } else {
      fputc('%', stderr);
      if (__c != '%') {
        __fmt = __p + 1;
        continue;
      }
    }
    __fmt = __p + 2;
  }
}
#endif

#ifdef __BIONIC__
#  include <syslog.h>
extern "C" void android_set_abort_message(const char* msg);
#endif // __BIONIC__

#if defined(__APPLE__) && __has_include(<CrashReporterClient.h>)
#   include <CrashReporterClient.h>
#   define _LIBCXXABI_USE_CRASHREPORTER_CLIENT
#endif

void __abort_message(const char* format, ...)
{
    // Write message to stderr. We do this before formatting into a
    // variable-size buffer so that we still get some information if
    // formatting into the variable-sized buffer fails.
#if !defined(NDEBUG) || !defined(LIBCXXABI_BAREMETAL)
    {
        va_list list;
        va_start(list, format);
#if defined(__I86__)
        fputs("libc++abi: ", stderr);
        __i86_vprint(format, list);
        fputc('\n', stderr);
#else
        fprintf(stderr, "libc++abi: ");
        vfprintf(stderr, format, list);
        fprintf(stderr, "\n");
#endif
        va_end(list);
    }
#endif

    // Format the arguments into an allocated buffer. We leak the buffer on
    // purpose, since we're about to abort() anyway.
#if defined(_LIBCXXABI_USE_CRASHREPORTER_CLIENT)
    char* buffer;
    va_list list;
    va_start(list, format);
    vasprintf(&buffer, format, list);
    va_end(list);

    CRSetCrashLogMessage(buffer);
#elif defined(__BIONIC__)
    char* buffer;
    va_list list;
    va_start(list, format);
    vasprintf(&buffer, format, list);
    va_end(list);

    // Show error in tombstone.
    android_set_abort_message(buffer);

    // Show error in logcat.
    openlog("libc++abi", 0, 0);
    syslog(LOG_CRIT, "%s", buffer);
    closelog();
#endif // __BIONIC__

    abort();
}
