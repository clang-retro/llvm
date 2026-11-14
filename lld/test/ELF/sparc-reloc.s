# REQUIRES: sparc
# RUN: llvm-mc -filetype=obj -triple=sparc %s -o %t.o
# RUN: ld.lld %t.o --defsym=c=0x01234567 --defsym=d=0xfff --defsym=e=-0x1000 --defsym=f=-0x80 -o %t
# RUN: llvm-readobj --file-headers %t | FileCheck --check-prefix=V8 %s
# RUN: llvm-objdump -d --no-show-raw-insn --no-print-imm-hex %t | FileCheck %s
# RUN: llvm-objdump -s %t | FileCheck --check-prefix=HEX %s
# RUN: ld.lld -m elf32_sparc %t.o --defsym=c=0x01234567 --defsym=d=0xfff --defsym=e=-0x1000 --defsym=f=-0x80 -o %t2
# RUN: cmp %t %t2

## An object built for V8+ (EM_SPARC32PLUS) links with plain V8 ones, and
## the output is V8+.
# RUN: llvm-mc -filetype=obj -triple=sparc -mattr=+v8plus %s -o %t.v8p.o
# RUN: llvm-readobj --file-headers %t.v8p.o | FileCheck --check-prefix=V8P %s
# RUN: ld.lld %t.o %t.v8p.o --defsym=c=0x01234567 --defsym=d=0xfff --defsym=e=-0x1000 --defsym=f=-0x80 \
# RUN:   --allow-multiple-definition -o %t3
# RUN: llvm-readobj --file-headers %t3 | FileCheck --check-prefix=V8P %s

# V8:      Class: 32-bit (0x1)
# V8:      Machine: EM_SPARC (0x2)
# V8:      Flags [ (0x0)
# V8-NEXT: ]

# V8P:      Class: 32-bit (0x1)
# V8P:      Machine: EM_SPARC32PLUS (0x12)
# V8P:      Flags [ (0x100)
# V8P-NEXT:   EF_SPARC_32PLUS (0x100)

.globl _start
_start:
  nop
bar:
  retl
  nop

## R_SPARC_HI22, R_SPARC_LO10
# CHECK-LABEL: section .ABS_32:
# CHECK:        sethi 18641, %o0
# CHECK-NEXT:   or %o0, 359, %o0
.section .ABS_32,"ax",@progbits
  sethi %hi(c), %o0
  or    %o0, %lo(c), %o0

## R_SPARC_13
# CHECK-LABEL: section .ABS_13:
# CHECK:        mov 4095, %o0
# CHECK-NEXT:   mov -4096, %o1
.section .ABS_13,"ax",@progbits
  or    %g0, d, %o0
  or    %g0, e, %o1

## R_SPARC_WDISP30, R_SPARC_WDISP22, to bar (0x200d8) in .text
# CHECK-LABEL: <foo>:
# CHECK-NEXT:   call 0x200d8
# CHECK-NEXT:   nop
# CHECK-NEXT:   ba 0x200d8
# CHECK-NEXT:   nop
.section .PC,"ax",@progbits
foo:
  call bar
  nop
  ba bar
  nop

## R_SPARC_32, R_SPARC_16, R_SPARC_8, R_SPARC_DISP32 (_start - 0x30108)
# HEX-LABEL: section .ABS_DATA:
# HEX-NEXT:  30100 01234567 0fff8000 fffeffcc
.section .ABS_DATA,"aw",@progbits
  .long c
  .half d
  .byte f
  .byte 0
  .long _start - .
