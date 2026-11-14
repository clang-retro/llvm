// Cross compiling for Solaris from another system with no GCC for Solaris:
// the driver links with ld.lld, the sysroot's crt1.o and libc, and the
// compiler-rt builtins, for programs built with picolibc's Solaris layer.
// UNSUPPORTED: system-solaris

// RUN: %clang -### %s --target=sparc-sun-solaris -mcpu=v8 -static \
// RUN:     --sysroot=%S/Inputs/solaris_cross_tree 2>&1 \
// RUN:   | FileCheck --check-prefixes=CHECK,STATIC %s
// RUN: %clang -### %s --target=sparc-sun-solaris -mcpu=v8 \
// RUN:     --sysroot=%S/Inputs/solaris_cross_tree 2>&1 \
// RUN:   | FileCheck --check-prefixes=CHECK,DYNAMIC %s

// CHECK:   "-cc1" "-triple" "sparc-sun-solaris"
// CHECK-SAME: "-internal-externc-isystem" "[[SYSROOT:[^"]+]]/usr/include"
// CHECK:   "{{[^"]*}}ld.lld"
// STATIC-SAME: "-static"
// CHECK-SAME: "--hash-style=sysv"
// DYNAMIC-SAME: "-dynamic-linker" "/usr/lib/ld.so.1"
// CHECK-SAME: "[[SYSROOT]]/usr/lib/crt1.o"
// CHECK-SAME: "-L[[SYSROOT]]/usr/lib"
// STATIC-SAME: "--start-group" "-lc" "{{[^"]*}}libclang_rt.builtins.a" "--end-group"
// DYNAMIC-SAME: "-lc" "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-NOT: crti.o
// CHECK-NOT: values-
