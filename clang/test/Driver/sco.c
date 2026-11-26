// Driver defaults for SCO UnixWare and OpenServer on 32-bit x86.

// RUN: %clang -### -c --target=i386-pc-sco %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CC
// CC: "-cc1" "-triple" "i386-pc-sco"
// CC-SAME: "-mrelocation-model" "static"
// CC-SAME: "-target-cpu" "i386"
// CC-SAME: "-internal-isystem" "{{[^"]*}}{{/|\\\\}}..{{/|\\\\}}i386-pc-sco{{/|\\\\}}usr{{/|\\\\}}include"
// CC-NOT: "/usr/include"

// RUN: %clang -### -c --target=i486-pc-sco3.2v5.0.7 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CC-486
// CC-486: "-target-cpu" "i486"

// Static programs, with crt1.o, -lc and the builtins from the sysroot.
// RUN: %clang -### --target=i386-pc-sco --sysroot=%S/Inputs/sco_tree \
// RUN:   -resource-dir=%S/Inputs/resource_dir %s -o hello 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LD
// LD: "{{[^"]*}}ld.lld{{(.exe)?}}" "-m" "elf_i386" "-static"
// LD-SAME: "--build-id=none" "-z" "norelro" "-z" "nognustack"
// LD-SAME: "--image-base" "0x8048000" "-o" "hello"
// LD-SAME: "{{[^"]*}}sco_tree{{/|\\\\}}usr{{/|\\\\}}lib{{/|\\\\}}crt1.o"
// LD-SAME: "-L{{[^"]*}}sco_tree{{/|\\\\}}usr{{/|\\\\}}lib"
// LD-SAME: "--start-group" "-lc"
// LD-SAME: "{{[^"]*}}sco_tree{{/|\\\\}}usr{{/|\\\\}}lib{{/|\\\\}}libclang_rt.builtins.a"
// LD-SAME: "--end-group"

// RUN: %clang -### --target=i386-pc-sco --sysroot=%S/Inputs/sco_tree \
// RUN:   -nostdlib %s -o hello 2>&1 | FileCheck %s --check-prefix=NOSTD
// NOSTD-NOT: crt1.o
// NOSTD-NOT: "-lc"
