# Tools

## llvm-elf2hunk

Turns an m68k ELF executable, linked with `--emit-relocs`, into an
AmigaOS Hunk executable that `LoadSeg()` can load. Each run of allocated
sections of the same kind (code, data or bss, in chip or any memory)
becomes one hunk, and each 32-bit absolute relocation becomes a
`HUNK_RELOC32` entry. The program starts at the first byte of the first
hunk. clang's `m68k-amigaos` driver runs it after linking.

## llvm-elf2tos

Turns an m68k ELF executable, linked with `--emit-relocs`, into an Atari
TOS program (`.PRG`, `.TOS`, `.TTP` or `.ACC`) that GEMDOS `Pexec()` can
load. The text, data and bss keep the layout they were linked with, and
each 32-bit absolute relocation becomes an entry in the fixup table.
clang's `m68k-atari-tos` driver runs it after linking.

## llvm-rc and llvm-windres for 16-bit Windows

`llvm-rc -win16` (or `llvm-windres --target=i8086-pc-win16`) compiles a
resource script to a 16-bit `.res` file: menus, dialogs, string tables,
accelerators, icons, cursors, bitmaps, version information and raw data,
with Windows 1.x and 2.x formats where they differ. Give the `.res` file
to clang with the objects and the linker puts the resources in the NE
file.

## The OMF writer

The MC layer writes OMF objects for the 8086: COMDATs for sections of
their own (so unused functions and data are dropped), several code
segments for a module whose far code is over 64 KiB, far calls as one
far-pointer fixup, and hashed names over 255 bytes.
