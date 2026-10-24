; Calls to printf and scanf with a constant format that needs no floating
; point go to picolibc's integer-only variants.
; RUN: llc -mtriple=i8086-pc-msdos < %s | FileCheck %s

@.int = private constant [8 x i8] c"%d %ld\0A\00"
@.str = private constant [8 x i8] c"%-10s%%\00"
@.ll = private constant [6 x i8] c"%lld\0A\00"
@.flt = private constant [4 x i8] c"%f\0A\00"
@.pos = private constant [5 x i8] c"%1$d\00"
@.wide = private constant [4 x i8] c"%ls\00"
@.set = private constant [9 x i8] c"%[^]]%ld\00"
@.sfl = private constant [3 x i8] c"%g\00"

declare i16 @printf(ptr, ...)
declare i16 @fprintf(ptr, ptr, ...)
declare i16 @snprintf(ptr, i16, ptr, ...)
declare i16 @sscanf(ptr, ptr, ...)
declare i16 @vprintf(ptr, ptr)

; CHECK-LABEL: _integers:
; CHECK: callw ___i_printf{{$}}
; CHECK: callw ___i_fprintf{{$}}
; CHECK: callw ___i_snprintf{{$}}
define void @integers(ptr %f, ptr %buf, ptr %s) {
  call i16 (ptr, ...) @printf(ptr @.int, i16 1, i32 2)
  call i16 (ptr, ptr, ...) @fprintf(ptr %f, ptr @.str, ptr %s)
  call i16 (ptr, i16, ptr, ...) @snprintf(ptr %buf, i16 8, ptr @.int, i16 1, i32 2)
  ret void
}

; CHECK-LABEL: _longlong:
; CHECK: callw ___l_printf{{$}}
define void @longlong() {
  call i16 (ptr, ...) @printf(ptr @.ll, i64 1)
  ret void
}

; CHECK-LABEL: _full:
; CHECK: callw _printf{{$}}
; CHECK: callw _printf{{$}}
; CHECK: callw _printf{{$}}
; CHECK: callw _printf{{$}}
; CHECK: callw _vprintf{{$}}
define void @full(ptr %fmt, ptr %ap) {
  call i16 (ptr, ...) @printf(ptr @.flt, double 1.0)
  call i16 (ptr, ...) @printf(ptr @.pos, i16 1)
  call i16 (ptr, ...) @printf(ptr @.wide, ptr null)
  call i16 (ptr, ...) @printf(ptr %fmt, i16 1)
  call i16 @vprintf(ptr @.int, ptr %ap)
  ret void
}

; CHECK-LABEL: _scan:
; CHECK: callw ___i_sscanf{{$}}
; CHECK: callw _sscanf{{$}}
define void @scan(ptr %in, ptr %a, ptr %b) {
  call i16 (ptr, ptr, ...) @sscanf(ptr %in, ptr @.set, ptr %a, ptr %b)
  call i16 (ptr, ptr, ...) @sscanf(ptr %in, ptr @.sfl, ptr %a)
  ret void
}
