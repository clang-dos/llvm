; MinGW targets for Windows before Vista use emulated TLS by default: their
; loaders set up native TLS only for the program and the DLLs loaded with it.
; RUN: llc < %s -mtriple=i686-pc-windows5.1-gnu | FileCheck --check-prefix=EMU %s
; RUN: llc < %s -mtriple=i486-pc-windows4.0-gnu | FileCheck --check-prefix=EMU %s
; RUN: llc < %s -mtriple=i686-pc-windows6.0-gnu | FileCheck --check-prefix=NATIVE %s
; RUN: llc < %s -mtriple=i686-w64-windows-gnu | FileCheck --check-prefix=NATIVE %s
; RUN: llc < %s -mtriple=i686-pc-windows5.1-gnu -emulated-tls=0 | FileCheck --check-prefix=NATIVE %s

@x = thread_local global i32 1

define i32 @get() {
  %v = load i32, ptr @x
  ret i32 %v
}

; EMU: calll ___emutls_get_address
; NATIVE-NOT: emutls
; NATIVE: __tls_index
