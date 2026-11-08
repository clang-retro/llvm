# Building

## The toolchain package

The [clang-dos toolchain
package](https://clang-retro.github.io/dos-sdk/toolchain/) has a build of
this repository with every sysroot. Its nightly release is rebuilt from
`main` here.

## Building the fork

Build it as upstream LLVM, with clang and lld, and the X86, PowerPC and
the experimental M68k targets:

```sh
cmake -G Ninja -S llvm -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_TARGETS_TO_BUILD="X86;PowerPC" \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=M68k
ninja -C build clang lld llvm-ar llvm-ranlib llvm-nm llvm-rc \
  llvm-elf2hunk llvm-elf2tos
```

dos-sdk's
[`packaging/build-llvm.sh`](https://github.com/clang-retro/dos-sdk/blob/main/packaging/build-llvm.sh)
is the script the packages are built with, for Linux and (cross-built
with MinGW) Windows hosts. Then build the sysroots as each SDK describes,
or all of them with dos-sdk's
[`packaging/build-sysroot.sh`](https://clang-retro.github.io/dos-sdk/dev/ci/).

## CI

`.github/workflows/clang-dos-toolchain.yml` builds the toolchain package
with each pull request's commit and runs its smoke test on every target,
using dos-sdk's toolchain workflow. A push to `main` builds both hosts and
publishes the LLVM tools in the `llvm-prebuilt` release, where the
toolchain workflows of dos-sdk, picolibc and JWlink find them instead of
building LLVM again.

## Tests

The fork's changes come with lit tests next to upstream's, under
`llvm/test` and `clang/test`; run them with `ninja check-llvm` and
`ninja check-clang` as usual.
