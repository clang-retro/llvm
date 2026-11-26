// The 32-bit DOS toolchain (i386-pc-msdos): ELF objects linked into LE
// programs, with the DOS/32A extender of the sysroot as the stub, so the
// program runs without a separate extender. The sysroot is i386-msdos.

// RUN: %clang --target=i386-pc-msdos --sysroot=%S/Inputs/msdos32_tree \
// RUN:   -fuse-ld=wlink -### %s 2>&1 | FileCheck %s --check-prefix=LE
// LE:      "-cc1" "-triple" "i386-pc-msdos"
// LE-SAME: "-target-cpu" "i386"
// LE-SAME: "-internal-isystem" "{{.*}}msdos32_tree{{/|\\\\}}include"
// LE:      "{{.*}}wlink" "format" "os2" "le" "option" "quiet" "option" "eliminate" "name" "a.exe"
// LE-SAME: "option" "stub={{.*}}msdos32_tree{{/|\\\\}}lib{{/|\\\\}}dos32a.exe"
// LE-SAME: "option" "stack=65536"
// LE-SAME: "file" "{{.*}}msdos32_tree{{/|\\\\}}lib{{/|\\\\}}crt0.o" "option" "start=_start"
// LE-SAME: "library" "{{.*}}msdos32_tree{{/|\\\\}}lib{{/|\\\\}}libc.a"
// LE-SAME: "library" "{{.*}}msdos32_tree{{/|\\\\}}lib{{/|\\\\}}libclang_rt.builtins.a"

// Without dos32a.exe in the sysroot, the linker's own stub stays.
// RUN: %clang --target=i386-pc-msdos --sysroot=%S/Inputs/os2_tree \
// RUN:   -fuse-ld=wlink -### %s 2>&1 | FileCheck %s --check-prefix=NOSTUB
// NOSTUB:     "format" "os2" "le"
// NOSTUB-NOT: "stub=

// The 16-bit triple keeps the 16-bit toolchain.
// RUN: %clang --target=i8086-pc-msdos --sysroot=%S/Inputs/msdos_tree \
// RUN:   -fuse-ld=wlink -### %s 2>&1 | FileCheck %s --check-prefix=MZ
// MZ: "{{.*}}wlink" "format" "dos"

// The CPU of the arch name, without SSE.
// RUN: %clang --target=i686-pc-msdos -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=I686
// I686: "-target-cpu" "pentiumpro"

// Code is not position-independent.
// RUN: %clang --target=i386-pc-msdos -fPIC -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NOPIC
// NOPIC-NOT: "-pic-level"

// C++ has exceptions and RTTI, unwound through SjLj function contexts, and
// __thread variables use emulated TLS.
// RUN: %clangxx --target=i386-pc-msdos -### -c -x c++ %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CXX
// CXX: "-cc1"
// CXX-SAME: "-femulated-tls"
// CXX-SAME: "-fcxx-exceptions"
// CXX-SAME: "-exception-model=sjlj"
// CXX-NOT: "-fno-rtti"

// C++ programs link libc++, libc++abi and the unwinder.
// RUN: %clangxx --target=i386-pc-msdos --sysroot=%S/Inputs/msdos32_tree \
// RUN:   -fuse-ld=wlink -### -x c++ %s 2>&1 | FileCheck %s --check-prefix=LIBCXX
// LIBCXX: "-internal-isystem" "{{.*}}msdos32_tree{{/|\\\\}}include{{/|\\\\}}c++{{/|\\\\}}v1"
// LIBCXX: "{{.*}}wlink"
// LIBCXX-SAME: "library" "{{.*}}msdos32_tree{{/|\\\\}}lib{{/|\\\\}}libunwind.a"
// LIBCXX-SAME: "library" "{{.*}}msdos32_tree{{/|\\\\}}lib{{/|\\\\}}libc.a"
