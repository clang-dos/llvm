//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP_SRC_IOSTREAM_OBJECTS_H
#define _LIBCPP_SRC_IOSTREAM_OBJECTS_H

#include "std_stream.h"

#include <__config>
#include <__memory/construct_at.h>
#include <__ostream/basic_ostream.h>
#include <fstream>
#include <istream>

// The standard stream objects are defined in iostream.cpp. On i8086 the wide
// ones are in wiostream.cpp instead, so that a program that does not use them
// does not link them and the wide character facets they need.
#if _LIBCPP_HAS_WIDE_CHARACTERS && defined(__I86__)
#  define _LIBCPP_SEPARATE_WIDE_STREAMS 1
#else
#  define _LIBCPP_SEPARATE_WIDE_STREAMS 0
#endif

// cin is in cin.cpp on i8086, for the same reason: a program that only writes
// does not link the input stream buffer or the reading it does.
#if defined(__I86__)
#  define _LIBCPP_SEPARATE_CIN 1
#else
#  define _LIBCPP_SEPARATE_CIN 0
#endif

#define ABI_NAMESPACE_STR _LIBCPP_TOSTRING(_LIBCPP_ABI_NAMESPACE)

_LIBCPP_BEGIN_NAMESPACE_STD
_LIBCPP_BEGIN_EXPLICIT_ABI_ANNOTATIONS

// This file implements the various stream objects provided inside <iostream>. We're doing some ODR violations in here,
// so this quite fragile. Specifically, the size of the stream objects (i.e. cout, cin etc.) needs to stay the same.
// For that reason, we have `stream` and `stream_data` separated into two objects. The public `stream` objects only
// contain the actual stream, while the private `stream_data` objects contains the `basic_streambuf` we're using as well
// as the mbstate_t. `stream_data` objects are only accessible within the library, so they aren't ABI sensitive and we
// can change them as we want.

template <class StreamT>
union stream {
  constexpr stream() {}
  stream(const stream&)            = delete;
  stream& operator=(const stream&) = delete;
  constexpr ~stream() {}

  StreamT value;
};

template <class StreamT, class SyncBufT, class UnsyncBufT>
union stream_data {
  constexpr stream_data() {}
  constexpr ~stream_data() {}
  struct {
    union {
      SyncBufT sync_buffer;
      UnsyncBufT unsync_buffer;
    };
    mbstate_t mb;
  };
};

template <class StreamT, class SyncBufT, class UnsyncBufT>
void init_stream(FILE* stdstream, stream<StreamT>& stream, stream_data<StreamT, SyncBufT, UnsyncBufT>& data) {
  data.mb = {};
  std::construct_at(&data.sync_buffer, stdstream, &data.mb);
  std::construct_at(&stream.value, &data.sync_buffer);
}

template <class StreamT, class SyncBufT, class UnsyncBufT>
void switch_to_sync_stream(FILE* stdstream, stream<StreamT>& stream, stream_data<StreamT, SyncBufT, UnsyncBufT>& data) {
  data.unsync_buffer.__adopt_file(nullptr, {}); // reset the file, so that basic_filebuf doesn't close standard streams
  std::destroy_at(&data.unsync_buffer);
  data.mb = {};
  std::construct_at(&data.sync_buffer, stdstream, &data.mb);
  stream.value.rdbuf(&data.sync_buffer);
}

template <class StreamT, class SyncBufT, class UnsyncBufT>
void switch_to_unsync_stream(FILE* stdstream,
                             stream<StreamT>& stream,
                             stream_data<StreamT, SyncBufT, UnsyncBufT>& data,
                             ios_base::openmode mode) {
  std::destroy_at(&data.sync_buffer);
  std::construct_at(&data.unsync_buffer);
  data.unsync_buffer.__adopt_file(stdstream, mode);
  stream.value.rdbuf(&data.unsync_buffer);
}

#define CHAR_MANGLING_char "D"
#define CHAR_MANGLING_wchar_t "_W"
#define CHAR_MANGLING(CharT) CHAR_MANGLING_##CharT

#ifdef _LIBCPP_COMPILER_CLANG_BASED
#  define STRING_DATA_CONSTINIT constinit
#else
#  define STRING_DATA_CONSTINIT
#endif

#ifdef _LIBCPP_ABI_MICROSOFT
#  define STREAM(StreamT, SyncBufT, UnsyncBufT, CharT, var)                                                            \
    STRING_DATA_CONSTINIT stream_data<StreamT<CharT>, SyncBufT<CharT>, UnsyncBufT<CharT>> var##_data;                  \
    _LIBCPP_EXPORTED_FROM_ABI STRING_DATA_CONSTINIT stream<StreamT<CharT>> var __asm__(                                \
        "?" #var "@" ABI_NAMESPACE_STR "@std@@3V?$" #StreamT                                                           \
        "@" CHAR_MANGLING(CharT) "U?$char_traits@" CHAR_MANGLING(CharT) "@" ABI_NAMESPACE_STR "@std@@@12@A")
#else
#  define STREAM(StreamT, SyncBufT, UnsyncBufT, CharT, var)                                                            \
    STRING_DATA_CONSTINIT stream_data<StreamT<CharT>, SyncBufT<CharT>, UnsyncBufT<CharT>> var##_data;                  \
    _LIBCPP_EXPORTED_FROM_ABI STRING_DATA_CONSTINIT stream<StreamT<CharT>> var
#endif

// These definitions and the declarations in <iostream> technically cause ODR violations, since they have different
// types (stream_data and {i,o}stream respectively). This means that <iostream> should never be included in this TU.

#if _LIBCPP_HAS_FILESYSTEM
template <class CharT>
using unsync_buffer = basic_filebuf<CharT>;
#else
// If we don't have access to files we can't treat the standard streams as files either - just save a dummy object in
// that case.
template <class>
using unsync_buffer = char;
#endif

#if _LIBCPP_SEPARATE_WIDE_STREAMS
// Set by wiostream.cpp when the wide streams are linked.
extern _LIBCPP_HIDDEN void (*__sync_wide_streams_with_stdio)(bool);
#endif

#if _LIBCPP_SEPARATE_CIN
// cin.cpp sets these when cin is linked, so that sync_with_stdio can switch
// its buffer. They point to data, so that a program which uses cin but never
// calls sync_with_stdio does not link the switching code and basic_filebuf.
using cin_stream_data = stream_data<basic_istream<char>, __stdinbuf<char>, unsync_buffer<char> >;
extern _LIBCPP_HIDDEN stream<basic_istream<char> >* __cin_for_sync;
extern _LIBCPP_HIDDEN cin_stream_data* __cin_data_for_sync;
// stdin, as cin.cpp saw it. Naming stdin in iostream.cpp would link stdin's
// FILE, read() and their text mode code into programs that only write.
extern _LIBCPP_HIDDEN FILE* __cin_file_for_sync;
#endif

_LIBCPP_END_EXPLICIT_ABI_ANNOTATIONS
_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP_SRC_IOSTREAM_OBJECTS_H
