; Compiling IR straight to an OMF object: C names get a leading underscore,
; globals become PUBDEF/EXTDEF/COMDEF records, and debug info is dropped.
; RUN: llc -mtriple=i8086-pc-msdos -filetype=obj %s -o %t.obj
; RUN: %python %S/../../../MC/X86/i8086/Inputs/omfdump.py %t.obj | FileCheck %s

; CHECK: SEGDEF _TEXT align=byte combine=public
; CHECK: SEGDEF _DATA align=word combine=public
; CHECK: SEGDEF _BSS align=word combine=public
; CHECK: GRPDEF DGROUP
; CHECK: COMDEF '_shared' near size=4
; CHECK: EXTDEF '_ext'
; CHECK: PUBDEF '_get' group=0 seg=1
; CHECK: PUBDEF '_value' group=1
; CHECK: PUBDEF '_zero' group=1
; CHECK: LEDATA seg=1
; CHECK:   FIXUP {{.*}} seg offset16 frame=F1:1
; CHECK:   FIXUP {{.*}} self offset16 frame=F5 target=T6
; CHECK: MODEND main=0

@value = global i16 7, align 2
@zero = global i16 0, align 2
@shared = common global i32 0, align 2

declare i16 @ext(i16)

define i16 @get() {
  %v = load i16, ptr @value
  %z = load i16, ptr @zero
  %s = add i16 %v, %z
  %r = call i16 @ext(i16 %s)
  store i32 1, ptr @shared
  ret i16 %r
}
