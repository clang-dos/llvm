//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// cin on i8086: a program that never reads it does not link it, its stream
// buffer or the reading code behind them.

#include "iostream_objects.h"

#if _LIBCPP_SEPARATE_CIN

_LIBCPP_BEGIN_NAMESPACE_STD
_LIBCPP_BEGIN_EXPLICIT_ABI_ANNOTATIONS

STREAM(basic_istream, __stdinbuf, unsync_buffer, char, cin);

// Defined in iostream.cpp.
extern _LIBCPP_EXPORTED_FROM_ABI stream<basic_ostream<char> > cout;

class CinInit {
public:
  CinInit() {
    init_stream(stdin, cin, cin_data);
    cin.value.tie(&cout.value);
    __cin_for_sync      = &cin;
    __cin_data_for_sync = &cin_data;
    __cin_file_for_sync = stdin;
  }
};

#  include "cin_init.h"

_LIBCPP_END_EXPLICIT_ABI_ANNOTATIONS
_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP_SEPARATE_CIN
