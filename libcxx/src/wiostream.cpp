//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// The wide standard streams, when they are separate from the narrow ones in
// iostream.cpp: a program links them, their initialization and the wide
// character facets only when it uses one of them.

#include "iostream_objects.h"

#if _LIBCPP_SEPARATE_WIDE_STREAMS

_LIBCPP_BEGIN_NAMESPACE_STD
_LIBCPP_BEGIN_EXPLICIT_ABI_ANNOTATIONS

STREAM(basic_istream, __stdinbuf, unsync_buffer, wchar_t, wcin);
STREAM(basic_ostream, __stdoutbuf, unsync_buffer, wchar_t, wcout);
STREAM(basic_ostream, __stdoutbuf, unsync_buffer, wchar_t, wcerr);
STREAM(basic_ostream, __stdoutbuf, unsync_buffer, wchar_t, wclog);

static void sync_wide_streams(bool sync) {
#  if _LIBCPP_HAS_FILESYSTEM
  if (sync) {
    switch_to_sync_stream(stdin, wcin, wcin_data);
    switch_to_sync_stream(stdout, wcout, wcout_data);
    switch_to_sync_stream(stderr, wclog, wclog_data);
  } else {
    switch_to_unsync_stream(stdin, wcin, wcin_data, ios::in);
    switch_to_unsync_stream(stdout, wcout, wcout_data, ios::out);
    switch_to_unsync_stream(stderr, wclog, wclog_data, ios::out);
  }
#  else
  (void)sync;
#  endif
}

class WideIOSInit {
public:
  WideIOSInit() {
    init_stream(stdin, wcin, wcin_data);
    init_stream(stdout, wcout, wcout_data);
    init_stream(stderr, wcerr, wcerr_data);
    init_stream(stderr, wclog, wclog_data);

    wcin.value.tie(&wcout.value);
    std::unitbuf(wcerr.value);
    wcerr.value.tie(&wcout.value);

    __sync_wide_streams_with_stdio = sync_wide_streams;
  }
  ~WideIOSInit() {
    wcout.value.flush();
    wclog.value.flush();
  }
};

#  include "wiostream_init.h"

_LIBCPP_END_EXPLICIT_ABI_ANNOTATIONS
_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP_SEPARATE_WIDE_STREAMS
