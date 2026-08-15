# WIP nnrtld Decompilation

This is a WIP decompilation of the runtime link-editor (`rtld`) binary from version 21.4.0 of the Nintendo SDK. The specific `rtld` binary used for this project can be found in the free demo for *Rhythm Heaven Groove*. Other games using the same nnSdk version may also work, provided that they use the same compiler settings as `rtld`'s compilation is dependent on the game's compiler settings. For example, in games that use Profile-Guided Optimization (PGO) such as recent first-party Nintendo EPD games, `rtld` is also compiled with PGO which makes matching much more difficult. *Rhythm Heaven Groove* appears to be compiled with only `-O3`, meaning `rtld` is also less aggressively optimized in comparison.

`Include/rocrt.AssemblyOffset.h` and `Sources/rocrt/rocrt_Nso.cpp` are mostly taken from [Nintendo OSS](https://support.nintendo.com/jp/oss/index.html) with some modifications (GPLv2). `Sources/util/util_memcpy_aarch64.S` is taken from [ARM's optimized routines](https://github.com/ARM-software/optimized-routines/blob/master/string/aarch64/memcpy.S) (MIT).

This also serves as an `rtld` reimplementation that can be used for game mods and whatever else you may need it for.

Currently **does not** generate a matching binary, however, it should be mostly equivalent and functional (as in you should be able to boot a game with it). Only 64-bit binaries are supported.

Inspired by https://github.com/marysaka/oss-rtld but this does not use any code from there.

Thank you to the [Open-EAD project](https://github.com/open-ead) for much of the tooling.

## Project TODOs
- Match all functions (currently at 28/41)
  - This sounds better than it actually is since the 28 were mostly trivial functions
  - `memset`/`strcmp`/`strlen` probably weren't implemented in assembly since they seem to change between SDK versions
- Generate matching binary
  - Match function + data ordering (mostly done)
  - Match `.eh_frame`/`.eh_frame_hdr` entries
  - Match `.dynamic` entries
    - `-Wl,-z now` seems to add both a bind now entry and a flags entry to `.dynamic` but official rtld only has the flags entry
  - This will likely require manually overriding the build-id as it is calculated from all input sections, including those that get stripped out of the final NSO
- Better function names/organization
  - The nnSdk binary includes some similar functions that we can probably use the names of
  - TU splits are complete guesses, they probably aren't perfect

## Building

### Requirements
- CMake 3.13+
- Ninja (or whatever you want)
- Clang (w/ C++ 20 support)

```sh
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release --toolchain=cmake/toolchain.cmake

ninja -C build

# or if you'd rather use the setup script
python3 Tools/setup.py
```

For decompilation, the following are also required:
- Clang 16.0.0 (installed by `setup.py`)
  - Note that the prebuilt binary requires ncurses 5
- llvm-objdump
- Rust
- Python 3
- Original `rtld` NSO

```sh
python3 Tools/setup.py --nso-path <path_to_rtld_nso> --for-check

# to make sure everything is setup ok
Tools/check

# to check a specific function
Tools/check FunctionName -j EX # optional: -mw to automatically rebuild on file changes
```

## Some Notes
- changes from 20.x.x to 21.x.x
  - linker symbols like `__EX_start` are now marked with `__attribute__(__visibility__("hidden"))`
    - this can be seen in that they lost their `.got` entries and are instead accessed directly through `adr`
  - noreturn functions are marked as such (function pointers included)
    - this can be seen in that they no longer restore the stack before being called
    - some seem to have `udf 0x8002` at the end as a trap as well
    - c++'s `[[noreturn]]` doesn't seem to support function pointers so we'll use `__attribute__((__noreturn__))`
  - `nn::ro::detail::ArchData` sdk version was updated to match
  - `_init` calls a new function before `ProtectRelro` (it's a no-op though)
    - though this may have just been optimized out bc the 20.x.x rtld binaries I'm looking at were compiled with PGO