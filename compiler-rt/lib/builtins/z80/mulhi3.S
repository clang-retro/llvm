//===-- mulhi3.S - 16-bit multiplication ----------------------------------===//
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

// int __mulhi3(int a, int b): the low 16 bits of a * b, by shifts and adds
// over the bits of b, from the top one.
	.text
	.globl	__mulhi3
__mulhi3:
	push	bc
	ld	b, h
	ld	c, l
	ld	hl, 0
	ld	a, 16
1:	add	hl, hl
	ex	de, hl
	add	hl, hl
	ex	de, hl
	jp	nc, 2f
	add	hl, bc
2:	dec	a
	jp	nz, 1b
	pop	bc
	ret
