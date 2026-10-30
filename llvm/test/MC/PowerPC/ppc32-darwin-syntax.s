# RUN: llvm-mc -triple powerpc-apple-darwin8 %s | FileCheck %s

# Apple's assembler syntax: rN/fN/crN register names, lo16/hi16/ha16, and
# condition register bits as plain numbers (it has no names for them).

# CHECK: L1:
L1:
# CHECK-NEXT: addis r3, r2, ha16(_x-L1)
	addis r3, r2, ha16(_x-L1)
# CHECK-NEXT: lwz r3, lo16(_x-L1)(r3)
	lwz r3, lo16(_x-L1)(r3)
# CHECK-NEXT: lis r4, hi16(_x)
	lis r4, hi16(_x)
# CHECK-NEXT: fadd f1, f2, f3
	fadd f1, f2, f3
# CHECK-NEXT: cmpw cr7, r3, r4
	cmpw cr7, r3, r4
# CHECK-NEXT: bf 28, L1
	bc 4, 28, L1
# CHECK-NEXT: crand 0, 1, 2
	crand 0, 1, 2
