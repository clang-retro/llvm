# RUN: llvm-mc -triple=z80 -show-encoding %s | FileCheck %s
# RUN: not llvm-mc -triple=i8080 %s 2>&1 | FileCheck --check-prefix=I8080 %s

# The instructions the Z80 adds.

	ld	a, (ix+5)
# CHECK: ld a, (ix+5) ; encoding: [0xdd,0x7e,0x05]
# I8080: error: instruction requires a Z80
	ld	(iy-2), l
# CHECK: ld (iy-2), l ; encoding: [0xfd,0x75,0xfe]
	ld	ix, 0x1234
# CHECK: ld ix, 4660 ; encoding: [0xdd,0x21,0x34,0x12]
	add	ix, sp
# CHECK: add ix, sp ; encoding: [0xdd,0x39]
	push	ix
# CHECK: push ix ; encoding: [0xdd,0xe5]
	pop	iy
# CHECK: pop iy ; encoding: [0xfd,0xe1]
	sbc	hl, de
# CHECK: sbc hl, de ; encoding: [0xed,0x52]
	adc	hl, bc
# CHECK: adc hl, bc ; encoding: [0xed,0x4a]
	ld	de, (0x1234)
# CHECK: ld de, (4660) ; encoding: [0xed,0x5b,0x34,0x12]
	ld	(0x1234), bc
# CHECK: ld (4660), bc ; encoding: [0xed,0x43,0x34,0x12]
	srl	h
# CHECK: srl h ; encoding: [0xcb,0x3c]
	sra	b
# CHECK: sra b ; encoding: [0xcb,0x28]
	rr	l
# CHECK: rr l ; encoding: [0xcb,0x1d]
	bit	7, a
# CHECK: bit 7, a ; encoding: [0xcb,0x7f]
	jr	nz, .+2
# CHECK: jr nz,
	djnz	.+2
# CHECK: djnz
	neg
# CHECK: neg ; encoding: [0xed,0x44]
	ldir
# CHECK: ldir ; encoding: [0xed,0xb0]
