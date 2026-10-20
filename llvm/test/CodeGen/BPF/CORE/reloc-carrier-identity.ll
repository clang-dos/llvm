; RUN: opt -mtriple=bpfeb -O2 -S %s -o %t.ll
; RUN: FileCheck %s --check-prefix=IR < %t.ll
; RUN: llc -mtriple=bpfeb -filetype=obj %t.ll -o %t.o
; RUN: llvm-objdump --syms --no-print-imm-hex -dr --no-show-raw-insn %t.o | FileCheck %s --check-prefixes=CHECK,ROOT
; Read the second module from stdin so it has a distinct source filename.
; RUN: llvm-extract --func=read_member %s | opt -O2 -module-summary -o %t.first.bc
; RUN: llvm-extract --func=read_other_member < %s | opt -O2 -module-summary -o %t.second.bc
; RUN: llvm-link %t.first.bc %t.second.bc -o %t.linked.bc
; RUN: llc -mtriple=bpfel -filetype=obj %t.linked.bc -o %t.linked.o
; RUN: llvm-objdump --syms --no-print-imm-hex -dr --no-show-raw-insn %t.linked.o | FileCheck %s --check-prefix=ROOT
; RUN: llvm-lto -thinlto-action=thinlink %t.first.bc %t.second.bc -o %t.index.bc
; RUN: llvm-lto -thinlto-action=import %t.second.bc -thinlto-index=%t.index.bc -o - | opt -O2 | llc -mtriple=bpfel -filetype=obj -o %t.imported.o
; RUN: llvm-objdump -r --disassemble-symbols=read_other_member %t.imported.o | FileCheck %s --check-prefix=IMPORT

; A pointer access and a 32-bit offset query share a relocation payload and
; root but need differently sized globals. A type in another namespace has the
; same short name and access path, but its relocation must remain distinct
; after IR linking and ThinLTO import. Punctuation in the type name must not
; interfere with either the new or the legacy relocation encoding.

; IR-DAG: @[[OFFSET_I64_GLOBAL:[^ ]+]] = private externally_initialized constant i64 0, !llvm.preserve.access.index ![[ROOT_MD:[0-9]+]], !llvm.bpf.core.relocation ![[OFFSET:[0-9]+]]
; IR-DAG: @[[OFFSET_I32_GLOBAL:[^ ]+]] = private externally_initialized constant i32 0, !llvm.preserve.access.index ![[ROOT_MD]], !llvm.bpf.core.relocation ![[OFFSET]]
; IR-LABEL: define {{.*}}@read_member(
; IR: load i64, ptr @[[OFFSET_I64_GLOBAL]]
; IR-LABEL: define {{.*}}@member_offset(
; IR: load i32, ptr @[[OFFSET_I32_GLOBAL]]
; IR-DAG: ![[OFFSET]] = !{i32 0, i64 4, !"0:1"}

; ROOT-NOT: llvm.S<ns::Arg$>
; ROOT-LABEL: <read_member>:
; ROOT: {{[rw]}}0 = *(u32 *)(r1 + 4)
; ROOT-NEXT: CO-RE <byte_off> {{\[}}[[TYPE_ID:[0-9]+]]{{\]}} struct S<ns::Arg$>::b (0:1)
; CHECK-LABEL: <member_offset>:
; CHECK: {{[rw]}}0 = 4
; CHECK-NEXT: CO-RE <byte_off> {{\[}}[[TYPE_ID]]{{\]}} struct S<ns::Arg$>::b (0:1)
; ROOT-LABEL: <read_other_member>:
; ROOT: {{[rw][0-9]+}} = *(u32 *)(r{{[0-9]+}} + 4)
; ROOT-NEXT: CO-RE <byte_off> {{\[[0-9]+\]}} struct S<ns::Arg$>::c (0:1)
; CHECK-LABEL: <read_legacy_member>:
; CHECK: {{[rw]}}0 = *(u32 *)(r1 + 0)
; CHECK-NEXT: CO-RE <byte_off> {{\[}}[[TYPE_ID]]{{\]}} struct S<ns::Arg$>::a (0:0)

; IMPORT-DAG: CO-RE <byte_off> {{\[[0-9]+\]}} struct S<ns::Arg$>::b (0:1)
; IMPORT-DAG: CO-RE <byte_off> {{\[[0-9]+\]}} struct S<ns::Arg$>::c (0:1)

target triple = "bpf"

%struct.S = type { i32, i32 }

@"llvm.S<ns::Arg$>:0:0$0:0" = external global i64, !llvm.preserve.access.index !3 #0

define i32 @read_member(ptr %p) !dbg !7 {
  %field = call ptr @llvm.preserve.struct.access.index.p0.p0(ptr elementtype(%struct.S) %p, i32 1, i32 1), !llvm.preserve.access.index !3, !dbg !9
  %value = load i32, ptr %field, align 4, !dbg !9
  ret i32 %value, !dbg !9
}

define i32 @member_offset(ptr %p) !dbg !14 {
  %field = call ptr @llvm.preserve.struct.access.index.p0.p0(ptr elementtype(%struct.S) %p, i32 1, i32 1), !llvm.preserve.access.index !3, !dbg !15
  %offset = call i32 @llvm.bpf.preserve.field.info.p0(ptr %field, i64 0), !dbg !15
  ret i32 %offset, !dbg !15
}

define i32 @read_other_member(ptr %p, ptr %q) !dbg !20 {
  %field = call ptr @llvm.preserve.struct.access.index.p0.p0(ptr elementtype(%struct.S) %p, i32 1, i32 1), !llvm.preserve.access.index !16, !dbg !22
  %value = load i32, ptr %field, align 4, !dbg !22
  %other = call i32 @read_member(ptr %q), !dbg !22
  %sum = add i32 %value, %other
  ret i32 %sum, !dbg !22
}

define i32 @read_legacy_member(ptr %p) !dbg !24 {
  %offset = load i64, ptr @"llvm.S<ns::Arg$>:0:0$0:0", align 8
  %field = getelementptr i8, ptr %p, i64 %offset
  %value = load i32, ptr %field, align 4
  ret i32 %value
}

declare ptr @llvm.preserve.struct.access.index.p0.p0(ptr, i32 immarg, i32 immarg)
declare i32 @llvm.bpf.preserve.field.info.p0(ptr, i64 immarg)

attributes #0 = { "btf_ama" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!12}
!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !1, isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "test.cpp", directory: "/")
!2 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!3 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "S<ns::Arg$>", file: !1, size: 64, elements: !4)
!4 = !{!5, !6}
!5 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !3, file: !1, baseType: !2, size: 32)
!6 = !DIDerivedType(tag: DW_TAG_member, name: "b", scope: !3, file: !1, baseType: !2, size: 32, offset: 32)
!7 = distinct !DISubprogram(name: "read_member", scope: !1, file: !1, type: !8, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!8 = !DISubroutineType(types: !{!2, !13})
!9 = !DILocation(line: 1, scope: !7)
!12 = !{i32 2, !"Debug Info Version", i32 3}
!13 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !3, size: 64)
!14 = distinct !DISubprogram(name: "member_offset", scope: !1, file: !1, type: !8, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!15 = !DILocation(line: 3, scope: !14)
!16 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "S<ns::Arg$>", scope: !25, file: !1, size: 64, elements: !17)
!17 = !{!18, !19}
!18 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !16, file: !1, baseType: !2, size: 32)
!19 = !DIDerivedType(tag: DW_TAG_member, name: "c", scope: !16, file: !1, baseType: !2, size: 32, offset: 32)
!20 = distinct !DISubprogram(name: "read_other_member", scope: !1, file: !1, type: !21, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!21 = !DISubroutineType(types: !{!2, !23, !13})
!22 = !DILocation(line: 4, scope: !20)
!23 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !16, size: 64)
!24 = distinct !DISubprogram(name: "read_legacy_member", scope: !1, file: !1, type: !8, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!25 = !DINamespace(name: "other", scope: null)
