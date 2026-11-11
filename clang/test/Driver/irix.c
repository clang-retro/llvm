// Driver defaults for SGI IRIX on MIPS.

// RUN: %clang -### -c --target=mips-sgi-irix6.5 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CC-O32
// CC-O32: "-cc1" "-triple" "mips-sgi-irix6.5"
// CC-O32-SAME: "-mrelocation-model" "pic"
// CC-O32-SAME: "-target-cpu" "mips2"
// CC-O32-SAME: "-target-abi" "o32"
// CC-O32-SAME: "-internal-isystem" "{{[^"]*}}{{/|\\\\}}..{{/|\\\\}}mips-sgi-irix{{/|\\\\}}usr{{/|\\\\}}include"
// CC-O32-NOT: "/usr/include"

// RUN: %clang -### -c --target=mips64-sgi-irix6.5 -mabi=n32 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CC-N32
// CC-N32: "-target-cpu" "mips3"
// CC-N32-SAME: "-target-abi" "n32"

// Dynamic linking against IRIX's libc, with rld and the ABI's library
// directory.
// RUN: %clang -### --target=mips-sgi-irix6.5 --sysroot=%S/Inputs/irix_tree \
// RUN:   -resource-dir=%S/Inputs/resource_dir %s -o hello 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LD-O32
// LD-O32: "{{[^"]*}}ld.lld{{(.exe)?}}" "-m" "elf32btsmip" "--hash-style=sysv"
// LD-O32-SAME: "--build-id=none" "-z" "norelro" "-z" "nognustack"
// LD-O32-SAME: "-dynamic-linker" "/usr/lib/rld" "--image-base" "0x400000"
// LD-O32-SAME: "-e" "__start" "-o" "hello"
// LD-O32-SAME: "{{[^"]*}}irix_tree{{/|\\\\}}usr{{/|\\\\}}lib{{/|\\\\}}crt1.o"
// LD-O32-SAME: "-L{{[^"]*}}irix_tree{{/|\\\\}}usr{{/|\\\\}}lib"
// LD-O32-SAME: "-lc"
// LD-O32-SAME: "{{[^"]*}}irix_tree{{/|\\\\}}usr{{/|\\\\}}lib{{/|\\\\}}crtn.o"

// RUN: %clang -### --target=mips64-sgi-irix6.5 -mabi=n32 \
// RUN:   --sysroot=%S/Inputs/irix_tree %s -o hello 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LD-N32
// LD-N32: "-m" "elf32btsmipn32"
// LD-N32-SAME: "-dynamic-linker" "/usr/lib32/rld" "--image-base" "0x10000000"
// LD-N32-SAME: "{{[^"]*}}irix_tree{{/|\\\\}}usr{{/|\\\\}}lib32{{/|\\\\}}crt1.o"

// RUN: %clang -### --target=mips64-sgi-irix6.5 -mabi=64 \
// RUN:   --sysroot=%S/Inputs/irix_tree %s -o hello 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LD-N64
// LD-N64: "-m" "elf64btsmip"
// LD-N64-SAME: "-dynamic-linker" "/usr/lib64/rld" "--image-base" "0x10000000"
// LD-N64-SAME: "{{[^"]*}}irix_tree{{/|\\\\}}usr{{/|\\\\}}lib64{{/|\\\\}}crt1.o"

// Static programs, such as those built on picolibc, need no rld.
// RUN: %clang -### --target=mips-sgi-irix6.5 --sysroot=%S/Inputs/irix_tree \
// RUN:   -static %s -o hello 2>&1 | FileCheck %s --check-prefix=STATIC
// STATIC: "-static"
// STATIC-NOT: "-dynamic-linker"
// STATIC-SAME: "--start-group" "-lc"
// STATIC-SAME: "--end-group"

// Shared libraries have no startup files and no fixed base.
// RUN: %clang -### --target=mips-sgi-irix6.5 --sysroot=%S/Inputs/irix_tree \
// RUN:   -shared %s -o libfoo.so 2>&1 | FileCheck %s --check-prefix=SHARED
// SHARED: "-shared"
// SHARED-NOT: crt1.o
// SHARED-NOT: "--image-base"
