//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "iostream_objects.h"

_LIBCPP_BEGIN_NAMESPACE_STD
_LIBCPP_BEGIN_EXPLICIT_ABI_ANNOTATIONS

#if !_LIBCPP_SEPARATE_CIN
STREAM(basic_istream, __stdinbuf, unsync_buffer, char, cin);
#endif
STREAM(basic_ostream, __stdoutbuf, unsync_buffer, char, cout);
STREAM(basic_ostream, __stdoutbuf, unsync_buffer, char, cerr);
STREAM(basic_ostream, __stdoutbuf, unsync_buffer, char, clog);
#if _LIBCPP_HAS_WIDE_CHARACTERS && !_LIBCPP_SEPARATE_WIDE_STREAMS
STREAM(basic_istream, __stdinbuf, unsync_buffer, wchar_t, wcin);
STREAM(basic_ostream, __stdoutbuf, unsync_buffer, wchar_t, wcout);
STREAM(basic_ostream, __stdoutbuf, unsync_buffer, wchar_t, wcerr);
STREAM(basic_ostream, __stdoutbuf, unsync_buffer, wchar_t, wclog);
#endif // _LIBCPP_HAS_WIDE_CHARACTERS

// Pretend we're inside a system header so the compiler doesn't flag the use of the init_priority
// attribute with a value that's reserved for the implementation (we're the implementation).
#include "iostream_init.h"

// On Windows the TLS storage for locales needs to be initialized before we create
// the standard streams, otherwise it may not be alive during program termination
// when we flush the streams.
static void force_locale_initialization() {
#ifdef _WIN32
  static bool once = []() {
    auto loc = __locale::__newlocale(_LIBCPP_ALL_MASK, "C", 0);
    {
      __locale::__locale_guard g(loc); // forces initialization of locale TLS
      ((void)g);
    }
    __locale::__freelocale(loc);
    return true;
  }();
  ((void)once);
#endif
}

class DoIOSInit {
public:
  DoIOSInit();
  ~DoIOSInit();
};

DoIOSInit::DoIOSInit() {
  force_locale_initialization();

#if !_LIBCPP_SEPARATE_CIN
  init_stream(stdin, cin, cin_data);
#endif
  init_stream(stdout, cout, cout_data);
  init_stream(stderr, cerr, cerr_data);
  init_stream(stderr, clog, clog_data);

#if !_LIBCPP_SEPARATE_CIN
  cin.value.tie(&cout.value);
#endif
  std::unitbuf(cerr.value);
  cerr.value.tie(&cout.value);

#if _LIBCPP_HAS_WIDE_CHARACTERS && !_LIBCPP_SEPARATE_WIDE_STREAMS
  init_stream(stdin, wcin, wcin_data);
  init_stream(stdout, wcout, wcout_data);
  init_stream(stderr, wcerr, wcerr_data);
  init_stream(stderr, wclog, wclog_data);

  wcin.value.tie(&wcout.value);
  std::unitbuf(wcerr.value);
  wcerr.value.tie(&wcout.value);
#endif
}

DoIOSInit::~DoIOSInit() {
  cout.value.flush();
  clog.value.flush();

#if _LIBCPP_HAS_WIDE_CHARACTERS && !_LIBCPP_SEPARATE_WIDE_STREAMS
  wcout.value.flush();
  wclog.value.flush();
#endif
}

ios_base::Init::Init() {
  static DoIOSInit init_the_streams; // gets initialized once
}

ios_base::Init::~Init() {}

#if _LIBCPP_SEPARATE_WIDE_STREAMS
void (*__sync_wide_streams_with_stdio)(bool);
#endif
#if _LIBCPP_SEPARATE_CIN
stream<basic_istream<char> >* __cin_for_sync;
cin_stream_data* __cin_data_for_sync;
#endif

bool ios_base::sync_with_stdio(bool sync) {
  static bool previous_state = true;
  bool r                     = previous_state;

#if _LIBCPP_HAS_FILESYSTEM
  if (sync != previous_state) {
    if (sync) {
#  if !_LIBCPP_SEPARATE_CIN
      switch_to_sync_stream(stdin, cin, cin_data);
#  endif
      switch_to_sync_stream(stdout, cout, cout_data);
      switch_to_sync_stream(stderr, clog, clog_data);
#  if _LIBCPP_HAS_WIDE_CHARACTERS && !_LIBCPP_SEPARATE_WIDE_STREAMS
      switch_to_sync_stream(stdin, wcin, wcin_data);
      switch_to_sync_stream(stdout, wcout, wcout_data);
      switch_to_sync_stream(stderr, wclog, wclog_data);
#  endif
    } else {
#  if !_LIBCPP_SEPARATE_CIN
      switch_to_unsync_stream(stdin, cin, cin_data, ios::in);
#  endif
      switch_to_unsync_stream(stdout, cout, cout_data, ios::out);
      switch_to_unsync_stream(stderr, clog, clog_data, ios::out);
#  if _LIBCPP_HAS_WIDE_CHARACTERS && !_LIBCPP_SEPARATE_WIDE_STREAMS
      switch_to_unsync_stream(stdin, wcin, wcin_data, ios::in);
      switch_to_unsync_stream(stdout, wcout, wcout_data, ios::out);
      switch_to_unsync_stream(stderr, wclog, wclog_data, ios::out);
#  endif
    }
#  if _LIBCPP_SEPARATE_CIN
    if (__cin_for_sync) {
      if (sync)
        switch_to_sync_stream(stdin, *__cin_for_sync, *__cin_data_for_sync);
      else
        switch_to_unsync_stream(stdin, *__cin_for_sync, *__cin_data_for_sync, ios::in);
    }
#  endif
#  if _LIBCPP_SEPARATE_WIDE_STREAMS
    if (__sync_wide_streams_with_stdio)
      __sync_wide_streams_with_stdio(sync);
#  endif
  }
#endif // _LIBCPP_HAS_FILESYSTEM

  previous_state = sync;
  return r;
}

_LIBCPP_END_EXPLICIT_ABI_ANNOTATIONS
_LIBCPP_END_NAMESPACE_STD
