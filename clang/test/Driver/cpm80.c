// The z80-unknown-cpm and i8080-unknown-cpm toolchains: CP/M-80 programs are
// linked by ld.lld at 0100h with the sysroot's cpm80.ld, and llvm-objcopy
// writes the .COM file.

// RUN: %clang --target=z80-unknown-cpm --sysroot=%S/Inputs/cpm80_tree \
// RUN:   -### %s 2>&1 | FileCheck %s --check-prefix=Z80
// Z80:      "-cc1" "-triple" "z80-unknown-cpm"
// Z80-NOT:  "-mframe-pointer=all"
// Z80:      "-internal-isystem" "{{.*}}cpm80_tree{{/|\\\\}}include"
// Z80:      "{{.*}}ld.lld" "-m" "elf32z80" "-static"
// Z80-SAME: "--gc-sections" "-e" "_start"
// Z80-SAME: "-T" "{{.*}}cpm80_tree{{/|\\\\}}lib{{/|\\\\}}cpm80.ld"
// Z80-SAME: "-o" "[[ELF:.*\.elf]]"
// Z80-SAME: "{{.*}}cpm80_tree{{/|\\\\}}lib{{/|\\\\}}crt0.o"
// Z80-SAME: "--start-group" "-lc" "{{.*}}libclang_rt.builtins.a" "--end-group"
// Z80-NEXT: "{{.*}}llvm-objcopy{{(\.exe)?}}" "-O" "binary" "[[ELF]]" "a.com"

// RUN: %clang --target=i8080-unknown-cpm --sysroot=%S/Inputs/cpm80_tree \
// RUN:   -### %s -o hello.com 2>&1 | FileCheck %s --check-prefix=I8080
// I8080:      "-cc1" "-triple" "i8080-unknown-cpm"
// I8080:      "{{.*}}ld.lld" "-m" "elf32z80"
// I8080:      "{{.*}}llvm-objcopy{{(\.exe)?}}" "-O" "binary" "{{.*}}.elf" "hello.com"

// Without --sysroot, the sysroot is ../z80-cpm or ../i8080-cpm next to clang.
// RUN: %clang --target=z80-unknown-cpm -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ROOT-Z80
// ROOT-Z80: "-internal-isystem" "{{.*}}bin{{/|\\\\}}..{{/|\\\\}}z80-cpm{{/|\\\\}}include"
// RUN: %clang --target=i8080-unknown-cpm -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ROOT-I8080
// ROOT-I8080: "-internal-isystem" "{{.*}}bin{{/|\\\\}}..{{/|\\\\}}i8080-cpm{{/|\\\\}}include"

// -r stops at the ELF object.
// RUN: %clang --target=z80-unknown-cpm --sysroot=%S/Inputs/cpm80_tree \
// RUN:   -r -### %s -o part.o 2>&1 | FileCheck %s --check-prefix=REL
// REL:     "{{.*}}ld.lld" "-m" "elf32z80" "-r" "-o" "part.o"
// REL-NOT: llvm-objcopy

// -mdouble= goes to cc1 for these targets, as for AVR.
// RUN: %clang -### --target=z80-unknown-cpm -mdouble=64 -c %s 2>&1 | FileCheck %s --check-prefix=MDOUBLE
// RUN: %clang -### --target=i8080-unknown-cpm -mdouble=32 -c %s 2>&1 | FileCheck %s --check-prefix=MDOUBLE32
// MDOUBLE: "-cc1"{{.*}} "-mdouble=64"
// MDOUBLE32: "-cc1"{{.*}} "-mdouble=32"
