//===-- divmodhi.S - 16-bit division --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// For the Z80 and the 8080: only 8080 instructions. Arguments come in HL and
// DE, the result goes in HL; BC is kept, A, DE and the flags are not.
//
//===----------------------------------------------------------------------===//

	.text

// HL / DE: quotient in BC, remainder in HL.
udivmod:
	ld	b, h
	ld	c, l
	ld	hl, 0
	ld	a, 16
1:	push	af
	ld	a, c
	add	a, a
	ld	c, a
	ld	a, b
	adc	a, a
	ld	b, a
	ld	a, l
	adc	a, a
	ld	l, a
	ld	a, h
	adc	a, a
	ld	h, a
	jp	c, 4f
	ld	a, l
	sub	e
	ld	l, a
	ld	a, h
	sbc	a, d
	ld	h, a
	jp	nc, 2f
	add	hl, de
	jp	3f
4:	ld	a, l
	sub	e
	ld	l, a
	ld	a, h
	sbc	a, d
	ld	h, a
2:	inc	c
3:	pop	af
	dec	a
	jp	nz, 1b
	ret

	.globl	__udivhi3
__udivhi3:
	push	bc
	call	udivmod
	ld	h, b
	ld	l, c
	pop	bc
	ret

	.globl	__umodhi3
__umodhi3:
	push	bc
	call	udivmod
	pop	bc
	ret

negde:
	ld	a, e
	cpl
	ld	e, a
	ld	a, d
	cpl
	ld	d, a
	inc	de
	ret
neghl:
	ld	a, l
	cpl
	ld	l, a
	ld	a, h
	cpl
	ld	h, a
	inc	hl
	ret

// Make HL and DE positive. A gets the sign of the quotient in bit 7 and
// the sign of the dividend in bit 6.
sdivprep:
	ld	a, h
	xor	d
	and	0x80
	ld	b, a
	ld	a, h
	and	0x80
	jp	z, 1f
	call	neghl
	ld	a, b
	or	0x40
	ld	b, a
1:	ld	a, d
	and	0x80
	call	nz, negde
	ld	a, b
	ret

	.globl	__divhi3
__divhi3:
	push	bc
	call	sdivprep
	push	af
	call	udivmod
	ld	h, b
	ld	l, c
	pop	af
	pop	bc
	and	0x80
	ret	z
	jp	neghl

	.globl	__modhi3
__modhi3:
	push	bc
	call	sdivprep
	push	af
	call	udivmod
	pop	af
	pop	bc
	and	0x40
	ret	z
	jp	neghl
