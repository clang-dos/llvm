; RUN: llc < %s -mtriple=powerpc-apple-darwin8 -relocation-model=dynamic-no-pic \
; RUN:   | FileCheck %s --check-prefixes=CHECK,DYN
; RUN: llc < %s -mtriple=powerpc-apple-darwin8 -relocation-model=pic \
; RUN:   | FileCheck %s --check-prefixes=CHECK,PIC
; RUN: llc < %s -mtriple=powerpc-apple-darwin8 -relocation-model=static \
; RUN:   | FileCheck %s --check-prefix=STATIC

; Mac OS X's classic linker does not make call stubs, so calls that dyld may
; bind go through compiler-made "$stub"s and "$lazy_ptr"s, and globals from
; other images are read through "$non_lazy_ptr"s, as Apple's gcc does.

@ext = external global i32
@loc = internal global i32 5

declare i32 @puts(ptr)
declare void @bar()

define void @local_fn() nounwind {
  ret void
}

define i32 @use_globals() nounwind {
; CHECK-LABEL: _use_globals:
; DYN:         lis r2, ha16(L_ext$non_lazy_ptr)
; DYN-NEXT:    lwz r2, lo16(L_ext$non_lazy_ptr)(r2)
; DYN-NEXT:    lis r3, ha16(_loc)
; DYN-NEXT:    lwz r3, lo16(_loc)(r3)
; PIC:         bcl 20, 31, L1$pb
; PIC-NEXT:  L1$pb:
; PIC-NEXT:    mflr r2
; PIC-NEXT:    addis r3, r2, ha16(L_ext$non_lazy_ptr-L1$pb)
; PIC-NEXT:    lwz r3, lo16(L_ext$non_lazy_ptr-L1$pb)(r3)
; PIC-NEXT:    addis r2, r2, ha16(_loc-L1$pb)
; PIC-NEXT:    lwz r2, lo16(_loc-L1$pb)(r2)
; STATIC-LABEL: _use_globals:
; STATIC:        lis r2, ha16(_ext)
; STATIC-NOT:    non_lazy_ptr
  %v = load i32, ptr @ext
  %w = load i32, ptr @loc
  %s = add i32 %v, %w
  ret i32 %s
}

define void @calls(ptr %s) nounwind {
; CHECK-LABEL: _calls:
; CHECK:         bl L_puts$stub
; CHECK:         bl L_bar$stub
; CHECK:         bl _local_fn
; STATIC-LABEL: _calls:
; STATIC:        bl _puts
; STATIC:        bl _bar
; STATIC:        bl _local_fn
  call i32 @puts(ptr %s)
  call void @bar()
  call void @local_fn()
  ret void
}

; DYN:         .section __TEXT,__symbol_stub1,symbol_stubs,pure_instructions,16
; DYN:       L_puts$stub:
; DYN-NEXT:    .indirect_symbol _puts
; DYN-NEXT:    lis r11, ha16(L_puts$lazy_ptr)
; DYN-NEXT:    lwzu r12, lo16(L_puts$lazy_ptr)(r11)
; DYN-NEXT:    mtctr r12
; DYN-NEXT:    bctr
; DYN-NEXT:  L_bar$stub:

; PIC:         .section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
; PIC:       L_puts$stub:
; PIC-NEXT:    .indirect_symbol _puts
; PIC-NEXT:    mflr r0
; PIC-NEXT:    bcl 20, 31, L0$_puts
; PIC-NEXT:  L0$_puts:
; PIC-NEXT:    mflr r11
; PIC-NEXT:    addis r11, r11, ha16(L_puts$lazy_ptr-L0$_puts)
; PIC-NEXT:    mtlr r0
; PIC-NEXT:    lwzu r12, lo16(L_puts$lazy_ptr-L0$_puts)(r11)
; PIC-NEXT:    mtctr r12
; PIC-NEXT:    bctr
; PIC-NEXT:  L_bar$stub:

; CHECK:         .section __DATA,__la_symbol_ptr,lazy_symbol_pointers
; CHECK:       L_puts$lazy_ptr:
; CHECK-NEXT:    .indirect_symbol _puts
; CHECK-NEXT:    .long dyld_stub_binding_helper
; CHECK-NEXT:  L_bar$lazy_ptr:
; CHECK-NEXT:    .indirect_symbol _bar
; CHECK-NEXT:    .long dyld_stub_binding_helper

; CHECK:         .section __DATA,__nl_symbol_ptr,non_lazy_symbol_pointers
; CHECK:       L_ext$non_lazy_ptr:
; CHECK-NEXT:    .indirect_symbol _ext
; CHECK-NEXT:    .long 0
; CHECK:         .subsections_via_symbols

; STATIC-NOT:  $stub
; STATIC-NOT:  $lazy_ptr
