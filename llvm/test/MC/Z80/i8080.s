# RUN: llvm-mc -triple=i8080 -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=z80 -show-encoding %s | FileCheck %s

# The 8080 instructions, in Zilog syntax, on both processors.

	nop
# CHECK: nop ; encoding: [0x00]
	ld	a, b
# CHECK: ld	a, b ; encoding: [0x78]
	ld	b, 0x12
# CHECK: ld	b, 18 ; encoding: [0x06,0x12]
	ld	hl, 0x1234
# CHECK: ld	hl, 4660 ; encoding: [0x21,0x34,0x12]
	ld	a, (hl)
# CHECK: ld	a, (hl) ; encoding: [0x7e]
	ld	(hl), c
# CHECK: ld	(hl), c ; encoding: [0x71]
	ld	a, (de)
# CHECK: ld	a, (de) ; encoding: [0x1a]
	ld	(bc), a
# CHECK: ld	(bc), a ; encoding: [0x02]
	ld	a, (0x1234)
# CHECK: ld	a, (4660) ; encoding: [0x3a,0x34,0x12]
	ld	hl, (0x1234)
# CHECK: ld	hl, (4660) ; encoding: [0x2a,0x34,0x12]
	ld	(0x1234), hl
# CHECK: ld	(4660), hl ; encoding: [0x22,0x34,0x12]
	ld	sp, hl
# CHECK: ld	sp, hl ; encoding: [0xf9]
	add	a, c
# CHECK: add	a, c ; encoding: [0x81]
	sub	0x10
# CHECK: sub	16 ; encoding: [0xd6,0x10]
	and	e
# CHECK: and	e ; encoding: [0xa3]
	xor	a
# CHECK: xor	a ; encoding: [0xaf]
	or	(hl)
# CHECK: or	(hl) ; encoding: [0xb6]
	cp	0x20
# CHECK: cp	32 ; encoding: [0xfe,0x20]
	adc	a, h
# CHECK: adc	a, h ; encoding: [0x8c]
	sbc	a, 3
# CHECK: sbc	a, 3 ; encoding: [0xde,0x03]
	inc	b
# CHECK: inc	b ; encoding: [0x04]
	dec	(hl)
# CHECK: dec	(hl) ; encoding: [0x35]
	inc	de
# CHECK: inc	de ; encoding: [0x13]
	dec	sp
# CHECK: dec	sp ; encoding: [0x3b]
	add	hl, bc
# CHECK: add	hl, bc ; encoding: [0x09]
	add	hl, sp
# CHECK: add	hl, sp ; encoding: [0x39]
	push	af
# CHECK: push	af ; encoding: [0xf5]
	pop	de
# CHECK: pop	de ; encoding: [0xd1]
	ex	de, hl
# CHECK: ex	de, hl ; encoding: [0xeb]
	ex	(sp), hl
# CHECK: ex	(sp), hl ; encoding: [0xe3]
	rlca
# CHECK: rlca ; encoding: [0x07]
	rra
# CHECK: rra ; encoding: [0x1f]
	cpl
# CHECK: cpl ; encoding: [0x2f]
	scf
# CHECK: scf ; encoding: [0x37]
	jp	0x1234
# CHECK: jp	4660 ; encoding: [0xc3,0x34,0x12]
	jp	nz, 0x1234
# CHECK: jp	nz, 4660 ; encoding: [0xc2,0x34,0x12]
	jp	c, 0x1234
# CHECK: jp	c, 4660 ; encoding: [0xda,0x34,0x12]
	jp	(hl)
# CHECK: jp	(hl) ; encoding: [0xe9]
	call	0x0005
# CHECK: call	5 ; encoding: [0xcd,0x05,0x00]
	call	pe, 0x1234
# CHECK: call	pe, 4660 ; encoding: [0xec,0x34,0x12]
	ret
# CHECK: ret ; encoding: [0xc9]
	ret	z
# CHECK: ret	z ; encoding: [0xc8]
	rst	0x38
# CHECK: rst	56 ; encoding: [0xff]
	di
# CHECK: di ; encoding: [0xf3]
	ei
# CHECK: ei ; encoding: [0xfb]
	halt
# CHECK: halt ; encoding: [0x76]
