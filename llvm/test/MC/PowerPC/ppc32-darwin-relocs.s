# RUN: llvm-mc -triple powerpc-apple-darwin8 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r --macho-indirect-symbols %t.o | FileCheck %s
# RUN: llvm-objdump --macho -d %t.o | FileCheck %s --check-prefix=DIS

# The relocations and indirect symbol table match what Apple's cctools
# assembler (cctools-877.8) produces for the same input.

	.text
	.globl	_f
	.align	2
_f:
	mflr r0
	bcl 20,31,L1$pb
L1$pb:
	mflr r2
	addis r3,r2,ha16(L_ext$non_lazy_ptr-L1$pb)
	lwz r3,lo16(L_ext$non_lazy_ptr-L1$pb)(r3)
	addis r4,r2,ha16(_data+8-L1$pb)
	la r4,lo16(_data+8-L1$pb)(r4)
	lis r5,hi16(_data)
	ori r5,r5,lo16(_data)
	lis r6,ha16(_ext)
	lwz r6,lo16(_ext)(r6)
	bl L_puts$stub
	mtlr r0
	blr

	.section	__TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align	2
L_puts$stub:
	.indirect_symbol _puts
	mflr r0
	bcl 20,31,L0$_puts
L0$_puts:
	mflr r11
	addis r11,r11,ha16(L_puts$lazy_ptr-L0$_puts)
	mtlr r0
	lwzu r12,lo16(L_puts$lazy_ptr-L0$_puts)(r11)
	mtctr r12
	bctr

	.data
	.align	2
_data:
	.long	0, 1, 2
	.long	_f
	.long	_ext+4

	.section	__DATA,__la_symbol_ptr,lazy_symbol_pointers
	.align	2
L_puts$lazy_ptr:
	.indirect_symbol _puts
	.long	dyld_stub_binding_helper

	.section	__DATA,__nl_symbol_ptr,non_lazy_symbol_pointers
	.align	2
L_ext$non_lazy_ptr:
	.indirect_symbol _ext
	.long	0
	.subsections_via_symbols

# CHECK:      Format: Mach-O 32-bit ppc
# CHECK:      Section __text {
# CHECK-NEXT:   0x2C 1 2 0 PPC_RELOC_BR24 0 __picsymbolstub1
# CHECK-NEXT:   0x28 0 2 1 PPC_RELOC_LO16 0 _ext
# CHECK-NEXT:   0x0 0 2 0 PPC_RELOC_PAIR 0 -
# CHECK-NEXT:   0x24 0 2 1 PPC_RELOC_HA16 0 _ext
# CHECK-NEXT:   0x0 0 2 0 PPC_RELOC_PAIR 0 -
# CHECK-NEXT:   0x20 0 2 0 PPC_RELOC_LO16 0 __data
# CHECK-NEXT:   0x0 0 2 0 PPC_RELOC_PAIR 0 -
# CHECK-NEXT:   0x1C 0 2 0 PPC_RELOC_HI16 0 __data
# CHECK-NEXT:   0x58 0 2 0 PPC_RELOC_PAIR 0 -
# CHECK-NEXT:   0x18 0 2 n/a PPC_RELOC_LO16_SECTDIFF 1 0x58
# CHECK-NEXT:   0x0 0 2 n/a PPC_RELOC_PAIR 1 0x8
# CHECK-NEXT:   0x14 0 2 n/a PPC_RELOC_HA16_SECTDIFF 1 0x58
# CHECK-NEXT:   0x58 0 2 n/a PPC_RELOC_PAIR 1 0x8
# CHECK-NEXT:   0x10 0 2 n/a PPC_RELOC_LO16_SECTDIFF 1 0x70
# CHECK-NEXT:   0x0 0 2 n/a PPC_RELOC_PAIR 1 0x8
# CHECK-NEXT:   0xC 0 2 n/a PPC_RELOC_HA16_SECTDIFF 1 0x70
# CHECK-NEXT:   0x68 0 2 n/a PPC_RELOC_PAIR 1 0x8
# CHECK-NEXT: }
# CHECK-NEXT: Section __picsymbolstub1 {
# CHECK-NEXT:   0x14 0 2 n/a PPC_RELOC_LO16_SECTDIFF 1 0x6C
# CHECK-NEXT:   0x0 0 2 n/a PPC_RELOC_PAIR 1 0x40
# CHECK-NEXT:   0xC 0 2 n/a PPC_RELOC_HA16_SECTDIFF 1 0x6C
# CHECK-NEXT:   0x2C 0 2 n/a PPC_RELOC_PAIR 1 0x40
# CHECK-NEXT: }
# CHECK-NEXT: Section __data {
# CHECK-NEXT:   0x10 0 2 1 PPC_RELOC_VANILLA 0 _ext
# CHECK-NEXT:   0xC 0 2 0 PPC_RELOC_VANILLA 0 __text
# CHECK-NEXT: }
# CHECK-NEXT: Section __la_symbol_ptr {
# CHECK-NEXT:   0x0 0 2 1 PPC_RELOC_VANILLA 0 dyld_stub_binding_helper
# CHECK-NEXT: }

# CHECK:      Indirect Symbols {
# CHECK-NEXT:   Number: 3

# DIS:      _f:
# DIS:        addis r3, r2, 0
# DIS-NEXT:   lwz r3, 104(r3)
# DIS:        bl .+12
