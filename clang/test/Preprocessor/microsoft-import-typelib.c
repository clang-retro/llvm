// Visual C++'s #import of a type library runs the importer that -tlbimport
// names and includes the .tlh it writes.
// The importer here is a shell script.
// UNSUPPORTED: system-windows
// RUN: rm -rf %t && mkdir -p %t/out
// RUN: cp %S/Inputs/fake-tlbimport.sh %t/imp && chmod +x %t/imp
// RUN: echo -n MSFT > %t/lib.tlb
// RUN: echo -n MSFT > %t/sniffed.bin
// RUN: %clang_cc1 -E -fms-compatibility -tlbimport %t/imp -tlbimport-dir %t/out -I %t %s | FileCheck %s
// RUN: %clang_cc1 -E -tlbimport %t/imp -tlbimport-dir %t/out -I %t %s | FileCheck %s
// RUN: not %clang_cc1 -E -fms-compatibility -I %t -DNO_IMPORTER %s 2>&1 | FileCheck --check-prefix=NOTOOL %s
// RUN: not %clang_cc1 -E -fms-compatibility -tlbimport %t/imp -tlbimport-dir %t/out -I %t -DERRORS %s 2>&1 | FileCheck --check-prefix=ERR %s

#ifdef NO_IMPORTER
#import "lib.tlb"
// NOTOOL: error: cannot import type library 'lib.tlb': no type library importer (tlbimport) was found next to the compiler
#elif defined(ERRORS)
#import "lib.tlb" fail
// ERR: error: cannot import type library 'lib.tlb': tlbimport: cannot read the type library
#import "missing.tlb"
// ERR: error: cannot import type library 'missing.tlb': file not found
#import "progid:Scripting.FileSystemObject"
// ERR: error: cannot import type library 'progid:Scripting.FileSystemObject': type libraries named by ProgID or LIBID are found through the registry; name the file instead
#import "lib.tlb" warn
// ERR: warning: tlbimport: warning: unknown #import attribute warn ignored
#else

// The attributes are passed one argument each, split at spaces and commas,
// with the type library and the directories to find its imports in.
#import "lib.tlb" no_namespace rename("A", "B"), named_guids \
                  rename_namespace("N")
// CHECK: IMPORTED(-o {{.*}}out --print-tlh -L {{.*}} no_namespace rename("A","B") named_guids rename_namespace("N") {{.*}}lib.tlb)

// A second #import of the same library includes nothing more.
#import "lib.tlb"
// CHECK-NOT: IMPORTED

// A file of another name is a type library if it starts as one; a resource
// number is passed on.
#import "sniffed.bin\2"
// CHECK: IMPORTED({{.*}}sniffed.bin\2)
#endif
