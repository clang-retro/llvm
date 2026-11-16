//===-- call.S - indirect calls -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// For the Z80 and the 8080: only 8080 instructions. Arguments come in HL and
// DE, the result goes in HL; A, BC, DE and the flags are not kept.
//
//===----------------------------------------------------------------------===//

// The compiler calls __z80_call_bc to call the function whose address is in
// BC, the arguments being in HL, DE and on the stack: CALL pushes the return
// address, and this jumps to BC.
	.text
	.globl	__z80_call_bc
__z80_call_bc:
	push	bc
	ret
