// The i8086-unknown-cpm toolchain: CP/M-86 programs are linked like DOS
// ones, as a .COM file (tiny) or an .EXE file (small), which llvm-exe2cmd
// then makes a CMD file.

// RUN: %clang --target=i8086-unknown-cpm --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=jwlink -### %s 2>&1 | FileCheck %s --check-prefix=SMALL
// SMALL:      "-cc1" "-triple" "i8086-unknown-cpm"
// SMALL-SAME: "-target-abi" "small"
// SMALL:      "{{.*}}jwlink" "format" "dos" {{.*}}"name" "[[EXE:.*\.exe]]"
// SMALL-SAME: "file" "{{.*}}lib{{/|\\\\}}small{{/|\\\\}}crt0.obj"
// SMALL-NEXT: "{{.*}}llvm-exe2cmd{{(\.exe)?}}" "[[EXE]]" "-o" "a.cmd"

// RUN: %clang --target=i8086-unknown-cpm --sysroot=%S/Inputs/msdos_tree \
// RUN:   -mcmodel=tiny -fuse-ld=jwlink -### %s -o hello.cmd 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TINY
// TINY:      "-target-abi" "tiny"
// TINY:      "{{.*}}jwlink" "format" "dos" "com" {{.*}}"name" "[[COM:.*\.com]]"
// TINY-SAME: "file" "{{.*}}lib{{/|\\\\}}tiny{{/|\\\\}}crt0.obj"
// TINY-NEXT: "{{.*}}llvm-exe2cmd{{(\.exe)?}}" "[[COM]]" "-o" "hello.cmd"

// Without --sysroot, the sysroot is ../i8086-cpm next to clang.
// RUN: %clang --target=i8086-unknown-cpm -fuse-ld=jwlink -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ROOT
// ROOT: "-internal-isystem" "{{.*}}bin{{/|\\\\}}..{{/|\\\\}}i8086-cpm{{/|\\\\}}include"

// CP/M-86 can't fix up segments, so only the tiny and small models work.
// RUN: not %clang --target=i8086-unknown-cpm -mcmodel=large -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=BAD
// BAD: error: unsupported argument 'large' to option '-mcmodel=' for target 'i8086-unknown-cpm'

// No exceptions or RTTI by default, as for DOS.
// RUN: %clang --target=i8086-unknown-cpm -### -c -x c++ %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CXX
// CXX-NOT: "-fcxx-exceptions"
// CXX: "-fno-rtti"
