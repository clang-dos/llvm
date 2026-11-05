//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// The file functions for Windows 95, 98, ME, Win32s and NT before 2000: see
// include/win32_ansi_files.h.

#define _LIBCPP_WIN32_ANSI_FILES_IMPL
#include "include/win32_ansi_files.h"

#if defined(_LIBCPP_WIN32_ANSI_FILES)

#  include <errno.h>
#  include <fcntl.h>
#  include <stdarg.h>
#  include <string.h>

namespace {

DWORD version() {
  static DWORD v = GetVersion();
  return v;
}

// Windows 95, 98, ME and Win32s set the top bit of the version.
bool ansi_only() { return (version() & 0x80000000) != 0; }

FARPROC system_function(const char* dll, const char* name) {
  HMODULE module = GetModuleHandleA(dll);
  return module ? GetProcAddress(module, name) : nullptr;
}

// The W function of Windows NT, found at run time.
#  define NT_FUNCTION(type, name)                                                                                      \
    ([] {                                                                                                              \
      static auto f = reinterpret_cast<type>(system_function("kernel32.dll", #name));                                  \
      return f;                                                                                                        \
    }())

// A name in the ANSI code page. A name that the code page can't hold is
// refused, as it could name another file.
struct ansi {
  char s[MAX_PATH * 2];
  bool ok;
  explicit ansi(LPCWSTR w) {
    BOOL lossy = FALSE;
    int n      = WideCharToMultiByte(CP_ACP, 0, w, -1, s, sizeof(s), nullptr, &lossy);
    ok         = n > 0 && !lossy;
    if (!ok)
      SetLastError(n > 0 ? ERROR_INVALID_NAME : ERROR_FILENAME_EXCED_RANGE);
  }
};

// An ANSI string as UTF-16 in out[size], with the result of the functions
// that fill a buffer: its length without the null if it fits, otherwise the
// size it needs.
DWORD to_wide(const char* a, LPWSTR out, DWORD size) {
  int need = MultiByteToWideChar(CP_ACP, 0, a, -1, nullptr, 0);
  if (need <= 0)
    return 0;
  if (static_cast<DWORD>(need) > size || !out)
    return need;
  MultiByteToWideChar(CP_ACP, 0, a, -1, out, size);
  return need - 1;
}

void to_wide_find_data(const WIN32_FIND_DATAA& a, LPWIN32_FIND_DATAW w) {
  w->dwFileAttributes = a.dwFileAttributes;
  w->ftCreationTime   = a.ftCreationTime;
  w->ftLastAccessTime = a.ftLastAccessTime;
  w->ftLastWriteTime  = a.ftLastWriteTime;
  w->nFileSizeHigh    = a.nFileSizeHigh;
  w->nFileSizeLow     = a.nFileSizeLow;
  w->dwReserved0      = a.dwReserved0;
  w->dwReserved1      = a.dwReserved1;
  if (!MultiByteToWideChar(CP_ACP, 0, a.cFileName, -1, w->cFileName, MAX_PATH))
    w->cFileName[0] = 0;
  if (!MultiByteToWideChar(CP_ACP, 0, a.cAlternateFileName, -1, w->cAlternateFileName, 14))
    w->cAlternateFileName[0] = 0;
}

// Windows 9x knows only the generic rights and no FILE_SHARE_DELETE, which
// NT 3.51 and earlier refuse too.
DWORD share_mode(DWORD share) {
  if (ansi_only() || (version() & 0xff) < 4)
    share &= ~FILE_SHARE_DELETE;
  return share;
}

DWORD ansi_access(DWORD access) {
  DWORD a = access & (GENERIC_READ | GENERIC_WRITE);
  if (access & (FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | DELETE | GENERIC_ALL))
    a |= GENERIC_WRITE;
  if (access & (FILE_READ_DATA | FILE_READ_ATTRIBUTES | FILE_READ_EA | GENERIC_EXECUTE | GENERIC_ALL))
    a |= GENERIC_READ;
  return a;
}

} // namespace

extern "C" {

bool __libcpp_win32_ansi_only() { return ansi_only(); }

BOOL WINAPI __libcpp_CreateDirectoryW(LPCWSTR name, LPSECURITY_ATTRIBUTES sa) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&CreateDirectoryW), CreateDirectoryW)(name, sa);
  ansi a(name);
  return a.ok && CreateDirectoryA(a.s, sa);
}

HANDLE WINAPI __libcpp_CreateFileW(
    LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disposition, DWORD flags, HANDLE tmpl) {
  share = share_mode(share);
  if (!ansi_only())
    return NT_FUNCTION(decltype(&CreateFileW), CreateFileW)(name, access, share, sa, disposition, flags, tmpl);
  ansi a(name);
  if (!a.ok)
    return INVALID_HANDLE_VALUE;
  // Windows 9x can't open a directory, so it has no use for these.
  flags &= ~(FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT);
  return CreateFileA(a.s, ansi_access(access), share, sa, disposition, flags, tmpl);
}

// Hard links came with Windows 2000.
BOOL WINAPI __libcpp_CreateHardLinkW(LPCWSTR link, LPCWSTR target, LPSECURITY_ATTRIBUTES sa) {
  auto f = ansi_only() ? nullptr : NT_FUNCTION(decltype(&CreateHardLinkW), CreateHardLinkW);
  if (!f) {
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
  }
  return f(link, target, sa);
}

HANDLE WINAPI __libcpp_FindFirstFileW(LPCWSTR name, LPWIN32_FIND_DATAW data) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&FindFirstFileW), FindFirstFileW)(name, data);
  ansi a(name);
  if (!a.ok)
    return INVALID_HANDLE_VALUE;
  WIN32_FIND_DATAA da;
  HANDLE h = FindFirstFileA(a.s, &da);
  if (h != INVALID_HANDLE_VALUE)
    to_wide_find_data(da, data);
  return h;
}

BOOL WINAPI __libcpp_FindNextFileW(HANDLE h, LPWIN32_FIND_DATAW data) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&FindNextFileW), FindNextFileW)(h, data);
  WIN32_FIND_DATAA da;
  if (!FindNextFileA(h, &da))
    return FALSE;
  to_wide_find_data(da, data);
  return TRUE;
}

DWORD WINAPI __libcpp_GetCurrentDirectoryW(DWORD size, LPWSTR out) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&GetCurrentDirectoryW), GetCurrentDirectoryW)(size, out);
  char a[MAX_PATH * 2];
  DWORD n = GetCurrentDirectoryA(sizeof(a), a);
  if (!n || n >= sizeof(a))
    return 0;
  return to_wide(a, out, size);
}

// GetDiskFreeSpaceEx came with Windows 95 OSR2; before it, the cluster
// counts of the drive's root give the sizes, up to 2 GB.
BOOL WINAPI
__libcpp_GetDiskFreeSpaceExW(LPCWSTR name, PULARGE_INTEGER avail, PULARGE_INTEGER total, PULARGE_INTEGER free) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&GetDiskFreeSpaceExW), GetDiskFreeSpaceExW)(name, avail, total, free);
  ansi a(name);
  if (!a.ok)
    return FALSE;
  using ex_t     = BOOL(WINAPI*)(LPCSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);
  static auto ex = reinterpret_cast<ex_t>(system_function("kernel32.dll", "GetDiskFreeSpaceExA"));
  if (ex)
    return ex(a.s, avail, total, free);
  char full[MAX_PATH * 2];
  DWORD n = GetFullPathNameA(a.s, sizeof(full), full, nullptr);
  if (!n || n >= sizeof(full))
    return FALSE;
  // The root: "X:\" or "\\server\share\".
  char* end = full + 3;
  if (full[0] == '\\' && full[1] == '\\') {
    end = strchr(full + 2, '\\');
    end = end ? strchr(end + 1, '\\') : nullptr;
    end = end ? end + 1 : full + strlen(full);
  }
  char root[MAX_PATH * 2];
  size_t len = end - full;
  memcpy(root, full, len);
  root[len] = 0;
  if (len && root[len - 1] != '\\') {
    root[len]     = '\\';
    root[len + 1] = 0;
  }
  DWORD sectors, bytes, free_clusters, clusters;
  if (!GetDiskFreeSpaceA(root, &sectors, &bytes, &free_clusters, &clusters))
    return FALSE;
  ULONGLONG cluster = static_cast<ULONGLONG>(sectors) * bytes;
  if (avail)
    avail->QuadPart = cluster * free_clusters;
  if (total)
    total->QuadPart = cluster * clusters;
  if (free)
    free->QuadPart = cluster * free_clusters;
  return TRUE;
}

DWORD WINAPI __libcpp_GetFileAttributesW(LPCWSTR name) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&GetFileAttributesW), GetFileAttributesW)(name);
  ansi a(name);
  return a.ok ? GetFileAttributesA(a.s) : INVALID_FILE_ATTRIBUTES;
}

DWORD WINAPI __libcpp_GetFullPathNameW(LPCWSTR name, DWORD size, LPWSTR out, LPWSTR* file_part) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&GetFullPathNameW), GetFullPathNameW)(name, size, out, file_part);
  ansi a(name);
  if (!a.ok)
    return 0;
  char full[MAX_PATH * 2];
  DWORD n = GetFullPathNameA(a.s, sizeof(full), full, nullptr);
  if (!n || n >= sizeof(full))
    return 0;
  DWORD r = to_wide(full, out, size);
  if (file_part && r && r < size) {
    LPWSTR last = nullptr;
    for (LPWSTR p = out; *p; ++p)
      if (*p == L'\\' || *p == L'/')
        last = p + 1;
    *file_part = last && *last ? last : nullptr;
  }
  return r;
}

DWORD WINAPI __libcpp_GetTempPathW(DWORD size, LPWSTR out) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&GetTempPathW), GetTempPathW)(size, out);
  char a[MAX_PATH * 2];
  DWORD n = GetTempPathA(sizeof(a), a);
  if (!n || n >= sizeof(a))
    return 0;
  return to_wide(a, out, size);
}

// Windows 9x has MoveFileEx only as a stub. Replacing a file is deleting it
// and moving the other one, so another program can see neither for a moment.
BOOL WINAPI __libcpp_MoveFileExW(LPCWSTR from, LPCWSTR to, DWORD flags) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&MoveFileExW), MoveFileExW)(from, to, flags);
  ansi a(from), b(to);
  if (!a.ok || !b.ok)
    return FALSE;
  if (MoveFileA(a.s, b.s))
    return TRUE;
  DWORD e = GetLastError();
  if (!(flags & MOVEFILE_REPLACE_EXISTING) || (e != ERROR_ALREADY_EXISTS && e != ERROR_FILE_EXISTS))
    return FALSE;
  DWORD attr = GetFileAttributesA(b.s);
  if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
    SetLastError(e);
    return FALSE;
  }
  if (!DeleteFileA(b.s))
    return FALSE;
  return MoveFileA(a.s, b.s);
}

BOOL WINAPI __libcpp_RemoveDirectoryW(LPCWSTR name) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&RemoveDirectoryW), RemoveDirectoryW)(name);
  ansi a(name);
  return a.ok && RemoveDirectoryA(a.s);
}

BOOL WINAPI __libcpp_DeleteFileW(LPCWSTR name) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&DeleteFileW), DeleteFileW)(name);
  ansi a(name);
  return a.ok && DeleteFileA(a.s);
}

BOOL WINAPI __libcpp_SetCurrentDirectoryW(LPCWSTR name) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&SetCurrentDirectoryW), SetCurrentDirectoryW)(name);
  ansi a(name);
  return a.ok && SetCurrentDirectoryA(a.s);
}

BOOL WINAPI __libcpp_SetFileAttributesW(LPCWSTR name, DWORD attributes) {
  if (!ansi_only())
    return NT_FUNCTION(decltype(&SetFileAttributesW), SetFileAttributesW)(name, attributes);
  ansi a(name);
  return a.ok && SetFileAttributesA(a.s, attributes);
}

// SetFilePointerEx came with Windows 2000.
BOOL WINAPI __libcpp_SetFilePointerEx(HANDLE h, LARGE_INTEGER distance, PLARGE_INTEGER pos, DWORD method) {
  LONG high = distance.HighPart;
  SetLastError(NO_ERROR);
  DWORD low = SetFilePointer(h, distance.LowPart, &high, method);
  if (low == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
    return FALSE;
  if (pos) {
    pos->LowPart  = low;
    pos->HighPart = high;
  }
  return TRUE;
}

// GetSystemTimeAsFileTime came with Windows NT 3.5.
void WINAPI __libcpp_GetSystemTimeAsFileTime(LPFILETIME ft) {
  using get_t   = void(WINAPI*)(LPFILETIME);
  static auto f = reinterpret_cast<get_t>(system_function("kernel32.dll", "GetSystemTimeAsFileTime"));
  if (f)
    return f(ft);
  SYSTEMTIME st;
  GetSystemTime(&st);
  SystemTimeToFileTime(&st, ft);
}

#  if !_LIBCPP_LIBC_PICOLIBC // picolibc's own _wopen and _wfopen do this

// _wopen and _wfopen: the C runtime's own on Windows NT when it has them
// (msvcrt.dll does, crtdll.dll doesn't), otherwise the narrow ones with the
// name in the ANSI code page.
#  if __MSVCRT_VERSION__ != 0
#    define CRT_DLL "msvcrt.dll"
#  endif

int __cdecl __libcpp_wopen(const wchar_t* name, int flags, ...) {
  int mode = 0;
  if (flags & _O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);
  }
#  ifdef CRT_DLL
  if (!ansi_only()) {
    using wopen_t = int(__cdecl*)(const wchar_t*, int, ...);
    static auto f = reinterpret_cast<wopen_t>(system_function(CRT_DLL, "_wopen"));
    if (f)
      return f(name, flags, mode);
  }
#  endif
  ansi a(name);
  if (!a.ok) {
    errno = ENOENT;
    return -1;
  }
  return _open(a.s, flags, mode);
}

FILE* __cdecl __libcpp_wfopen(const wchar_t* name, const wchar_t* mode) {
#  ifdef CRT_DLL
  if (!ansi_only()) {
    using wfopen_t = FILE*(__cdecl*)(const wchar_t*, const wchar_t*);
    static auto f  = reinterpret_cast<wfopen_t>(system_function(CRT_DLL, "_wfopen"));
    if (f)
      return f(name, mode);
  }
#  endif
  ansi a(name);
  char m[16];
  size_t i = 0;
  for (; mode[i] && i + 1 < sizeof(m); ++i)
    m[i] = static_cast<char>(mode[i]);
  m[i] = 0;
  if (!a.ok) {
    errno = ENOENT;
    return nullptr;
  }
  return fopen(a.s, m);
}

#  endif // !_LIBCPP_LIBC_PICOLIBC

} // extern "C"

#endif // _LIBCPP_WIN32_ANSI_FILES
