//===-- shifthi.S - 16-bit shifts -----------------------------------------===//
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

// Shifts by a count in E (the low 4 bits; larger counts are undefined).
	.text
	.globl	__ashlhi3
__ashlhi3:
	ld	a, e
	and	15
	ret	z
1:	add	hl, hl
	dec	a
	jp	nz, 1b
	ret

	.globl	__lshrhi3
__lshrhi3:
	ld	a, e
	and	15
	ret	z
	push	bc
	ld	c, a
1:	ld	a, h
	or	a
	rra
	ld	h, a
	ld	a, l
	rra
	ld	l, a
	dec	c
	jp	nz, 1b
	pop	bc
	ret

	.globl	__ashrhi3
__ashrhi3:
	ld	a, e
	and	15
	ret	z
	push	bc
	ld	c, a
1:	ld	a, h
	rla
	ld	a, h
	rra
	ld	h, a
	ld	a, l
	rra
	ld	l, a
	dec	c
	jp	nz, 1b
	pop	bc
	ret
