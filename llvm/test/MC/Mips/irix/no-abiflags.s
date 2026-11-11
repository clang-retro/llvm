# IRIX predates .MIPS.abiflags, so IRIX objects leave it out. Other MIPS
# targets keep it.

# RUN: llvm-mc -filetype=obj -triple mips-sgi-irix6.5 %s -o - \
# RUN:   | llvm-readelf -S - | FileCheck %s --check-prefix=O32 \
# RUN:       --implicit-check-not=.MIPS.abiflags
# RUN: llvm-mc -filetype=obj -triple mips64-sgi-irix6.5 -target-abi n32 %s -o - \
# RUN:   | llvm-readelf -S - | FileCheck %s --check-prefix=O32 \
# RUN:       --implicit-check-not=.MIPS.abiflags
# RUN: llvm-mc -filetype=obj -triple mips64-sgi-irix6.5 %s -o - \
# RUN:   | llvm-readelf -S - | FileCheck %s --check-prefix=N64 \
# RUN:       --implicit-check-not=.MIPS.abiflags
# RUN: llvm-mc -filetype=obj -triple mips-unknown-linux-gnu %s -o - \
# RUN:   | llvm-readelf -S - | FileCheck %s --check-prefix=LINUX

# O32: .reginfo
# N64: .MIPS.options
# LINUX: .MIPS.abiflags

	.text
	nop
