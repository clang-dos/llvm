; RUN: llc -mtriple=powerpc-apple-macosx10.4 -O2 < %s | FileCheck %s --check-prefix=DARWIN
; RUN: llc -mtriple=powerpc-unknown-linux-gnu -O2 < %s | FileCheck %s --check-prefix=LINUX

; GlobalMerge is off by default for PowerPC Mac OS X. Merging string literals
; would take them out of __cstring, and ld64 coalesces __cfstring entries by
; the contents they point to.

@.str = private unnamed_addr constant [4 x i8] c"abc\00", section "__TEXT,__cstring,cstring_literals", align 1
@.str.1 = private unnamed_addr constant [4 x i8] c"xyz\00", section "__TEXT,__cstring,cstring_literals", align 1
@a = internal global i32 1, align 4
@b = internal global i32 2, align 4

declare void @use(ptr, ptr, ptr, ptr)

define void @f() {
  call void @use(ptr @.str, ptr @.str.1, ptr @a, ptr @b)
  ret void
}

; DARWIN-NOT: MergedGlobals
; DARWIN: L_.str:
; DARWIN-NEXT: .asciz "abc"
; DARWIN: L_.str.1:
; DARWIN-NEXT: .asciz "xyz"
; DARWIN-NOT: MergedGlobals

; LINUX: MergedGlobals
