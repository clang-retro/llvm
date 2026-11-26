# REQUIRES: z80
# RUN: llvm-mc -filetype=obj -triple=z80 -o %t.o %s
# RUN: ld.lld -m elf32z80 %t.o --defsym=abs8=0x56 --Ttext=0x100 \
# RUN:   --Tdata=0x200 -o %t
# RUN: llvm-objdump -s -j .text -j .data %t | FileCheck %s
# RUN: llvm-readelf -h %t | FileCheck %s --check-prefix=HDR
# RUN: not ld.lld -m elf32z80 %t.o --defsym=abs8=0x56 --Ttext=0x100 \
# RUN:   --Tdata=0x10000 -o /dev/null 2>&1 | FileCheck %s --check-prefix=RANGE

## Static linking for CP/M-80: 16-bit absolute addresses in code and data,
## and an 8-bit value.

  .text
  .globl _start
_start:
  ld hl, data
  call func
  ld a, abs8
  jp _start
func:
  ret

  .data
data:
  .word func
  .byte abs8

# CHECK:      Contents of section .text:
# CHECK-NEXT:  0100 210002cd 0b013e56 c30001c9
# CHECK:      Contents of section .data:
# CHECK-NEXT:  0200 0b0156

# HDR: Machine: Zilog Z80

# RANGE: error: {{.*}}:(.text+0x1): relocation R_Z80_16 out of range: 65536 is not in [-32768, 65535]; references section '.data'
