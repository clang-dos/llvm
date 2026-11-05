//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Windows 95, 98, ME and Win32s have the Unicode file functions only as
// stubs that fail, and no _wopen or _wfopen that works. With headers for
// them (_WIN32_WINNT below 0x0500), the library calls the functions declared
// here instead. On Windows NT they call the W function, found at run time so
// that programs for 9x don't import its stub. On the others they convert the
// names to the ANSI code page and call the A function. They also stand in for
// the few functions that Windows NT 4.0 and earlier lack: SetFilePointerEx
// (2000) and GetSystemTimeAsFileTime (NT 3.5).

#ifndef _LIBCPP_SRC_INCLUDE_WIN32_ANSI_FILES_H
#define _LIBCPP_SRC_INCLUDE_WIN32_ANSI_FILES_H

#include <__config>

#if defined(_WIN32) && defined(__MINGW32__)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <io.h>
#  include <stdio.h>
#  include <windows.h>
#endif

#if defined(_WIN32) && defined(__MINGW32__) && _WIN32_WINNT < 0x0500
#  define _LIBCPP_WIN32_ANSI_FILES 1

extern "C" {
BOOL WINAPI __libcpp_CreateDirectoryW(LPCWSTR, LPSECURITY_ATTRIBUTES);
HANDLE WINAPI __libcpp_CreateFileW(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
BOOL WINAPI __libcpp_CreateHardLinkW(LPCWSTR, LPCWSTR, LPSECURITY_ATTRIBUTES);
HANDLE WINAPI __libcpp_FindFirstFileW(LPCWSTR, LPWIN32_FIND_DATAW);
BOOL WINAPI __libcpp_FindNextFileW(HANDLE, LPWIN32_FIND_DATAW);
DWORD WINAPI __libcpp_GetCurrentDirectoryW(DWORD, LPWSTR);
BOOL WINAPI __libcpp_GetDiskFreeSpaceExW(LPCWSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);
DWORD WINAPI __libcpp_GetFileAttributesW(LPCWSTR);
DWORD WINAPI __libcpp_GetFullPathNameW(LPCWSTR, DWORD, LPWSTR, LPWSTR*);
DWORD WINAPI __libcpp_GetTempPathW(DWORD, LPWSTR);
BOOL WINAPI __libcpp_MoveFileExW(LPCWSTR, LPCWSTR, DWORD);
BOOL WINAPI __libcpp_RemoveDirectoryW(LPCWSTR);
BOOL WINAPI __libcpp_DeleteFileW(LPCWSTR);
BOOL WINAPI __libcpp_SetCurrentDirectoryW(LPCWSTR);
BOOL WINAPI __libcpp_SetFileAttributesW(LPCWSTR, DWORD);
BOOL WINAPI __libcpp_SetFilePointerEx(HANDLE, LARGE_INTEGER, PLARGE_INTEGER, DWORD);
void WINAPI __libcpp_GetSystemTimeAsFileTime(LPFILETIME);
#  if !_LIBCPP_LIBC_PICOLIBC
int __cdecl __libcpp_wopen(const wchar_t*, int, ...);
FILE* __cdecl __libcpp_wfopen(const wchar_t*, const wchar_t*);
#  endif
// True on Windows 95, 98, ME and Win32s.
bool __libcpp_win32_ansi_only();
}

#  ifndef _LIBCPP_WIN32_ANSI_FILES_IMPL
#    define CreateDirectoryW __libcpp_CreateDirectoryW
#    define CreateFileW __libcpp_CreateFileW
#    define CreateHardLinkW __libcpp_CreateHardLinkW
#    define FindFirstFileW __libcpp_FindFirstFileW
#    define FindNextFileW __libcpp_FindNextFileW
#    define GetCurrentDirectoryW __libcpp_GetCurrentDirectoryW
#    define GetDiskFreeSpaceExW __libcpp_GetDiskFreeSpaceExW
#    define GetFileAttributesW __libcpp_GetFileAttributesW
#    define GetFullPathNameW __libcpp_GetFullPathNameW
#    define GetTempPathW __libcpp_GetTempPathW
#    define MoveFileExW __libcpp_MoveFileExW
#    define RemoveDirectoryW __libcpp_RemoveDirectoryW
#    define DeleteFileW __libcpp_DeleteFileW
#    define SetCurrentDirectoryW __libcpp_SetCurrentDirectoryW
#    define SetFileAttributesW __libcpp_SetFileAttributesW
#    define SetFilePointerEx __libcpp_SetFilePointerEx
#    define GetSystemTimeAsFileTime __libcpp_GetSystemTimeAsFileTime
#    if !_LIBCPP_LIBC_PICOLIBC
#      define _wopen __libcpp_wopen
#      define _wfopen __libcpp_wfopen
#    endif
#  endif
#endif

#endif // _LIBCPP_SRC_INCLUDE_WIN32_ANSI_FILES_H
