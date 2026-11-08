# Targets

Each target has a toolchain in clang's driver, which finds the target's
sysroot, picks the linker and turns the linked program into the system's
executable format. The sysroot is `--sysroot=`, or by default a directory
named after the target next to clang's `bin/` directory, which is where
the [toolchain package](https://clang-retro.github.io/dos-sdk/toolchain/)
puts it.

| System | Triple | Objects | Linker | Output | SDK |
| --- | --- | --- | --- | --- | --- |
| MS-DOS | `i8086-pc-msdos` | OMF | JWlink or Open Watcom `wlink` | `.COM` (tiny model) or MZ `.EXE` | [dos-sdk](https://clang-retro.github.io/dos-sdk/) |
| Windows 1.x to 3.x | `i8086-pc-win16`, `i8086-pc-win16X.YY` | OMF | JWlink or `wlink` | NE program or DLL | [win16-sdk](https://clang-retro.github.io/win16-sdk/) |
| OS/2 1.x | `i8086-pc-os2` | OMF | JWlink or `wlink` | NE program | [os2-sdk](https://clang-retro.github.io/os2-sdk/) |
| OS/2 2.0 to 4.x, ArcaOS | `i386-pc-os2` | ELF | JWlink or `wlink` | LX program or DLL | [os2-sdk](https://clang-retro.github.io/os2-sdk/) |
| Win32s, NT 3.1 to 11, 95/98/ME | `i386-pc-windowsX.Y-gnu` (and `i486`, `i586`, `i686`, `x86_64`) | COFF | lld | PE | [win32-sdk](https://clang-retro.github.io/win32-sdk/) |
| AmigaOS (Kickstart 1.3 to 3.x) | `m68k-amigaos` | ELF | lld, then `llvm-elf2hunk` | Hunk executable | [amiga-sdk](https://clang-retro.github.io/amiga-sdk/) |
| Atari ST, STe, TT, Falcon | `m68k-atari-tos` | ELF | lld with `tos.ld`, then `llvm-elf2tos` | GEMDOS PRG | [atari-sdk](https://clang-retro.github.io/atari-sdk/) |
| Classic Mac OS, 68k | `m68k-apple-macos` | ELF | `<triple>-ld` (Retro68) | application | [mac-sdk](https://clang-retro.github.io/mac-sdk/) |
| Classic Mac OS, PowerPC | `powerpc-apple-macos9` | ELF | `<triple>-ld` (Retro68) | PEF application | [mac-sdk](https://clang-retro.github.io/mac-sdk/) |
| Mac OS X 10.0 to 10.5, PowerPC | `powerpc-apple-darwin`, `powerpc-apple-macosx10.X` | Mach-O | `ld64` | Mach-O | [dos-sdk: Mac OS X](https://clang-retro.github.io/dos-sdk/mac/macosx/) |

## MS-DOS, 16-bit Windows and OS/2 1.x

One toolchain builds all three. Code is compiled to OMF objects for the
memory model given by `-mcmodel=` (`tiny`, `small`, the default,
`medium`, `compact`, `large` or `huge`), and linked by a Watcom-style
linker: `--ld-path=`, or `-fuse-ld=wlink`, `jwlink` or a path, or else
`wlink` or `jwlink` next to clang or on `PATH`. The sysroot has one library
directory per model, `lib/<model>/`, with `crt0.obj` and the default
libraries; Windows uses `lib/win16/<model>/`, and OS/2 `lib/os2/<model>/`.

- `-shared` makes a Windows or OS/2 DLL.
- The version after `win16` in the triple is the oldest Windows the
  program runs on (`i8086-pc-win163.10` for 3.1, `win161.01` for 1.x),
  and selects the matching import list and NE header fields.
- `-msize-t=16` with `-mcmodel=huge` is the `huge16` model of Open Watcom.

The [dos-sdk documentation](https://clang-retro.github.io/dos-sdk/dos/)
describes the models, the keywords and the C library in detail.

## 32-bit OS/2

`i386-pc-os2` compiles to ELF objects and links them with JWlink's
`format os2 lx`. `-mwindows` marks a Presentation Manager program, and
`-shared` makes a DLL. `__thread` and `thread_local` variables use
emulated TLS (`__emutls_get_address`, in the C library), and C++
exceptions use SjLj function contexts, since an LX program keeps no
`.eh_frame` for a table-driven unwinder. On an 80386, which has no
`CMPXCHG` or `XADD`, atomics go through the `__sync_*` functions of the
builtins library, which yield to the other threads with `DosSleep` while
they wait.

## Windows

The Windows targets are upstream's MinGW ones, with the Windows version
in the triple: `i386-pc-windows4.0-gnu` is Windows 95 and NT 4.0,
`i586-pc-windows5.1-gnu` is XP. From the version, clang:

- targets the CPU of the arch name (`i386`, `i486`, `i586`, `i686`)
  instead of a Pentium 4, so there is no SSE on systems that do not save
  its registers;
- writes that subsystem and OS version into the program, since Windows 9x
  refuses programs that ask for 5.0 or later;
- gives NT 3.x a writable import table, and uses emulated TLS before
  Vista.

libc++, libc++abi and libunwind build for Windows 95 to XP, against
`msvcrt.dll`, `crtdll.dll` or picolibc. On i686 MinGW and Cygwin, C++
empty classes are passed on the stack, as GCC does.

## AmigaOS and Atari TOS

Both link m68k ELF objects with `ld.lld --emit-relocs` and convert the
result: `llvm-elf2hunk` writes an AmigaOS Hunk executable that
`LoadSeg()` loads, and `llvm-elf2tos` a TOS program that `Pexec()` loads.
Code is for the 68000 unless `-mcpu=` says otherwise. The Atari ABI is
that of gcc's `m68k-atari-mint`: `int` is 32 bits and everything of two
bytes or more is aligned to 2.

## Classic Mac OS

`m68k-apple-macos` and `powerpc-apple-macos9` use the headers and
libraries of a sysroot (by default `<bin>/../<arch>-apple-macos`) and link
with `<triple>-ld`, as Retro68's toolchain provides it. On 68k, `pascal`
functions use the Pascal calling convention, and Toolbox calls compile to
their trap words with `__attribute__((raw_inline(...)))`.

## Mac OS X on PowerPC

The fork restores 32-bit PowerPC Darwin code generation, which upstream
LLVM dropped, and links programs the way Mac OS X 10.0 needs.
