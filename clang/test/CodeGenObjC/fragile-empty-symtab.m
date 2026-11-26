// The Objective-C runtime of Mac OS X 10.0 reads the symbol table of each
// module without a null check. Before 10.4, a module without classes or
// categories gets an empty symbol table. From 10.4, its pointer is null.

// RUN: %clang_cc1 -triple powerpc-apple-macosx10.0 -fobjc-runtime=macosx-fragile-10.0 -emit-llvm -o - %s | FileCheck --check-prefix=OLD %s
// RUN: %clang_cc1 -triple i386-apple-macosx10.3 -fobjc-runtime=macosx-fragile-10.3 -emit-llvm -o - %s | FileCheck --check-prefix=OLD %s
// RUN: %clang_cc1 -triple powerpc-apple-macosx10.4 -fobjc-runtime=macosx-fragile-10.4 -emit-llvm -o - %s | FileCheck --check-prefix=NEW %s

// OLD: @OBJC_SYMBOLS = private global {{.*}} section "__OBJC,__symbols,regular,no_dead_strip"
// OLD: @OBJC_MODULES = private global {{.*}} ptr @OBJC_SYMBOLS }, section "__OBJC,__module_info,regular,no_dead_strip"

// NEW-NOT: @OBJC_SYMBOLS
// NEW: @OBJC_MODULES = private global {{.*}} ptr null }, section "__OBJC,__module_info,regular,no_dead_strip"

@interface Root
+ (id)make;
@end

/// Sends a message, so that the module has Objective-C metadata.
id use(void) { return [Root make]; }
