// The clang-retro AIX SDK: -fuse-ld=lld links static programs with xcoff-ld,
// the SDK's crt0.o and C library, and the list of /unix system calls.

// RUN: %clang -### --target=powerpc-ibm-aix4.3 -fuse-ld=lld \
// RUN:   --sysroot=%S/Inputs/aix_retro_tree %s -o %t 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LINK
// LINK:      "-cc1" "-triple" "powerpc-ibm-aix4.3"
// LINK-SAME: "-target-cpu" "601"
// LINK-SAME: "-isysroot" "{{.*}}aix_retro_tree"
// LINK:      "{{.*}}xcoff-ld{{(.exe)?}}" "-b32" "-bpT:0x10000000" "-bpD:0x20000000"
// LINK-SAME: "-o" "{{[^"]*}}"
// LINK-SAME: "{{.*}}aix_retro_tree{{/|\\\\}}usr{{/|\\\\}}lib{{/|\\\\}}crt0.o"
// LINK-SAME: "-L{{.*}}aix_retro_tree/usr/lib" "-L{{.*}}aix_retro_tree/lib"
// LINK-SAME: "{{.*}}.o" "-lc"
// LINK-SAME: "{{.*}}aix_retro_tree{{/|\\\\}}usr{{/|\\\\}}lib{{/|\\\\}}libclang_rt.builtins.a"
// LINK-SAME: "-bI:{{.*}}aix_retro_tree{{/|\\\\}}usr{{/|\\\\}}lib{{/|\\\\}}unix.exp"
// LINK-SAME: "-bE:{{.*}}aix_retro_tree{{/|\\\\}}usr{{/|\\\\}}lib{{/|\\\\}}libc.exp"
// LINK-NOT:  "-bcdtors

// RUN: %clang -### --target=powerpc-ibm-aix4.3 -fuse-ld=lld -nostdlib \
// RUN:   --sysroot=%S/Inputs/aix_retro_tree %s -o %t 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NOSTDLIB
// NOSTDLIB:     "{{.*}}xcoff-ld{{(.exe)?}}"
// NOSTDLIB-NOT: crt0.o
// NOSTDLIB-NOT: "-lc"
// NOSTDLIB-NOT: "-bI:
// NOSTDLIB-NOT: "-bE:

// The default processor follows the AIX release.
// RUN: %clang -### -c --target=powerpc-ibm-aix6.1 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=PWR4
// PWR4: "-target-cpu" "pwr4"
// RUN: %clang -### -c --target=powerpc-ibm-aix7.1 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=PWR4
// RUN: %clang -### -c --target=powerpc-ibm-aix7.2 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=PWR7
// RUN: %clang -### -c --target=powerpc-ibm-aix %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=PWR7
// PWR7: "-target-cpu" "pwr7"

// Without -fuse-ld=lld, the AIX system linker as before.
// RUN: %clang -### --target=powerpc-ibm-aix4.3 \
// RUN:   --sysroot=%S/Inputs/aix_retro_tree %s -o %t 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SYSTEM
// SYSTEM-NOT: xcoff-ld
// SYSTEM:     "-bcdtors:mbr:0:s"
