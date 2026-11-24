# DWARF sections become segments of class DWARF, after the other segments.
# They are written with the 32-bit records (SEGDEF32, LEDATA32 and
# FIXUPP32). An offset into a DWARF section is a 32-bit offset fixup. An
# address in a DWARF section is a far pointer (the offset, then the
# segment). Each function COMDAT is the target of its addresses.
# RUN: llvm-mc -triple=i8086-pc-msdos -filetype=obj %s -o %t.obj
# RUN: %python %S/Inputs/omfdump.py %t.obj | FileCheck %s

# CHECK:      SEGDEF _BSS align=byte combine=public big=0 use32=0 len=0000 class=BSS
# CHECK-NEXT: SEGDEF32 .debug_abbrev align=byte combine=public big=0 use32=1 len=0001 class=DWARF
# CHECK-NEXT: SEGDEF32 .debug_str align=byte combine=public big=0 use32=1 len=0002 class=DWARF
# CHECK-NEXT: SEGDEF32 .debug_info align=byte combine=public big=0 use32=1 len=0012 class=DWARF
# CHECK-NEXT: GRPDEF DGROUP segs=[2, 3, 4]
# CHECK:      LEDATA32 seg=5 off=0000 len=1 00
# CHECK-NEXT: LEDATA32 seg=6 off=0000 len=2 6600
# CHECK-NEXT: LEDATA32 seg=7 off=0000 len=18 000000000000000000000000010000000100
# CHECK-NEXT:   FIXUP @000 seg offset32 frame=F5 target=T4:5 disp=None
# CHECK-NEXT:   FIXUP @004 seg offset32 frame=F5 target=T4:6 disp=None
# CHECK-NEXT:   FIXUP @008 seg ptr16:16 frame=F5 target=T6:1 disp=None
# CHECK-NEXT:   FIXUP @00C seg ptr16:16 frame=F5 target=T6:1 disp=None
# CHECK:      COMDAT 'f' flags=00 attr=00 align=1 group=0 seg=1 off=0000 len=1 c3

	.section .text.f,"ax",@progbits
	.globl	f
f:
	retw
.Lend:

	.section .debug_abbrev,"",@progbits
.Labbrev:
	.byte	0

	.section .debug_str,"MS",@progbits,1
.Lname:
	.asciz	"f"

	.section .debug_info,"",@progbits
	.long	.Labbrev
	.long	.Lname
	.long	f
	.long	.Lend
	.short	.Lend-f
