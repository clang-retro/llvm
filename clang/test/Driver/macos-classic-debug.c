// Debug information for classic Mac OS. PowerPC objects are XCOFF, which has
// sections only for DWARF v2 to v4: -g makes DWARF v4 there, and the options
// that need DWARF v5 or split DWARF are errors. 68k objects are ELF and keep
// the usual default (DWARF v5).

// RUN: %clang -### -c -g --target=powerpc-apple-macos9 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=PPC
// RUN: %clang -### -c -g --target=powerpc-apple-macos9.2 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=PPC
// PPC: "-dwarf-version=4"

// RUN: %clang -### -c -gdwarf-3 --target=powerpc-apple-macos9 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=PPC-V3
// PPC-V3: "-dwarf-version=3"

// RUN: not %clang -### -c -gdwarf-5 --target=powerpc-apple-macos9 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=PPC-V5
// RUN: not %clang -### -c -gdwarf-6 --target=powerpc-apple-macos9 %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=PPC-V6
// PPC-V5: error: unsupported option '-gdwarf-5' for target 'powerpc-apple-macos9'
// PPC-V6: error: unsupported option '-gdwarf-6' for target 'powerpc-apple-macos9'

// RUN: not %clang -### -c -g -gsplit-dwarf --target=powerpc-apple-macos9 %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=PPC-SPLIT
// PPC-SPLIT: error: unsupported option '-gsplit-dwarf' for target 'powerpc-apple-macos9'

// AIX also uses XCOFF: -gdwarf-6 is an error there too.
// RUN: not %clang -### -c -gdwarf-6 --target=powerpc-ibm-aix %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=AIX-V6
// AIX-V6: error: unsupported option '-gdwarf-6' for target 'powerpc-ibm-aix'

// RUN: %clang -### -c -g --target=m68k-apple-macos %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=M68K
// M68K: "-dwarf-version=5"
