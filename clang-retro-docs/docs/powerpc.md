# PowerPC

## Classic Mac OS

`powerpc-apple-macos9` builds code for PowerPC Macs, up to Mac OS 9, with the
same Toolbox headers as 68k; calls are imported from InterfaceLib and the
other shared libraries. Structures use 68k alignment, as on a real Mac.
See the [Classic Mac OS SDK](https://clang-retro.github.io/mac-sdk/).

## Mac OS X 10.0 to 10.5

Upstream LLVM removed 32-bit PowerPC Darwin. The fork restores its code
generation, and the driver links programs for Mac OS X 10.0 the way that
release needs. The tests boot Mac OS X in QEMU: see
[Mac OS X on PowerPC](https://clang-retro.github.io/dos-sdk/mac/macosx/).
