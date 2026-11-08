---
hide:
  - navigation
---

# clang-retro LLVM

This is [LLVM](https://llvm.org) and clang with support for 16 and 32-bit
systems that upstream LLVM does not target: MS-DOS and 16-bit Windows on
the 8086, OS/2, Windows from Win32s and NT 3.1 to 11, classic Mac OS on
68k and PowerPC, AmigaOS, the Atari ST family, and 32-bit Mac OS X on
PowerPC. These pages describe only what the fork adds. For everything
else, see the [upstream LLVM documentation](https://llvm.org/docs/) and
[clang's](https://clang.llvm.org/docs/).

```sh
clang --target=i8086-pc-msdos -mcmodel=large -Os app.c -o app.exe
clang --target=i8086-pc-win16 -Os hello.c -o hello.exe
clang --target=m68k-amigaos -O2 hello.c -o hello
clang --target=m68k-atari-tos -O2 gemhello.c -o gemhello.prg
clang --target=i386-pc-os2 -Os hello.c -o hello.exe
```

<div class="grid cards" markdown>

-   :material-target: **Targets**

    ---

    [Every target the fork adds](targets.md), with its triple, object
    format, linker and default sysroot.

-   :material-chip: **The 8086 backend**

    ---

    [16-bit x86](i8086.md): memory models, near, far and huge pointers,
    OMF objects, Win16 code and C++.

-   :material-alpha-m-box: **68k and PowerPC**

    ---

    [The M68k fixes](m68k.md) behind AmigaOS, Atari TOS and classic
    Mac OS, and [PowerPC](powerpc.md) for Mac OS 9 and Mac OS X.

-   :material-tools: **Tools**

    ---

    [llvm-elf2hunk, llvm-elf2tos, llvm-rc for Win16](tools.md) and the
    other additions to the LLVM tools and lld.

-   :material-hammer-wrench: **Build it**

    ---

    [Build the fork](building.md), or use the prebuilt toolchain package.

-   :material-view-grid-outline: **The SDKs**

    ---

    Headers, C libraries and examples for each system:
    [all clang-retro projects](https://clang-retro.github.io/dos-sdk/projects/).

</div>

## Getting it

Most people want the [clang-dos toolchain
package](https://clang-retro.github.io/dos-sdk/toolchain/), which has
this clang, lld, JWlink and a sysroot for every target, for Linux and
Windows hosts. Its nightly build follows the `main` branch here.

## License

The fork keeps LLVM's license, the Apache License v2.0 with LLVM
Exceptions, for its changes too, so that they can go upstream. See
[LICENSE.TXT](https://github.com/clang-retro/llvm/blob/main/LICENSE.TXT).
