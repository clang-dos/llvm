; dllexport (__export) definitions get an EXPDEF record (COMENT class A0,
; subtype 02) each, for a Windows linker. DOS programs have no exports, so
; an msdos target writes none.
; RUN: llc -mtriple=i8086-unknown-unknown-omf -filetype=obj %s -o %t.obj
; RUN: %python %S/../../../MC/X86/i8086/Inputs/omfdump.py %t.obj | FileCheck %s
; RUN: llc -mtriple=i8086-pc-msdos -filetype=obj %s -o %t.dos.obj
; RUN: %python %S/../../../MC/X86/i8086/Inputs/omfdump.py %t.dos.obj \
; RUN:   | FileCheck %s --check-prefix=DOS

; CHECK-DAG: PUBDEF 'WNDPROC'
; CHECK-DAG: PUBDEF '_counter'
; CHECK-DAG: COMENT class=A0 b'\x02\x00\x07WNDPROC\x00'
; CHECK-DAG: COMENT class=A0 b'\x02\x00\x08_counter\x00'
; CHECK-NOT: COMENT class=A0
; CHECK-NOT: SEGDEF .omf.exports
; DOS-NOT:   class=A0
; DOS-NOT:   .omf.exports

@counter = dllexport global i16 0
declare dllexport i16 @imported()

define dllexport x86_16_pascalcc i16 @wndproc(i16 %a) {
  ret i16 %a
}

define i16 @local() {
  ret i16 0
}
