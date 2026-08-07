# nnrtld

shitty rtld decompilation (well, reimplementation currently bc decompilation is hard)

currently based on the rtld binary shipped with SDK version 21.4.0 (the build id seems to vary by game/version, but everything else is identical - I haven't checked how lld generates the build id, but it likely includes parts of the ELF that aren't included in the NSO)

rtld appears to be compiled (or at least linked) alongside the game, so it follows optimization flags from the game (i.e. if a game is compiled with PGO, then rltd is also compiled with PGO)

most functions do not match, but I would like to eventually get them there

function names and TU splits are mostly just guesses - some are based on the `rocrt` from [Nintendo OSS](https://support.nintendo.com/jp/oss/index.html)

compiler is clang 14.0.x, but if matching isn't a concern, it should build with any clang version that supports c++20

TODO:
- match functions
- match data sections (.bss is matching, the others need some work)
  - `-Wl,-z now` seems to add both a bind now entry and a flags entry to `.dynamic` but official rtld only has the flags entry
  - check which functions need `.eh_frame` entries
- unified code style
- build instructions + tools for checking matches/progress
- check sdk for functions to align names as best as possible
  - it's possible some functions may look similar but have similar names because rtld has separate implementations

inspired by https://github.com/marysaka/oss-rtld but this does not use any code from there

notes:
- changes from 20.x.x to 21.x.x
  - linker symbols like `__EX_start` are now marked with `__attribute__(__visibility__("hidden"))`
    - this can be seen in that they lost their `.got` entries and are instead accessed directly through `adr`
  - noreturn functions are marked as such (function pointers included)
    - this can be seen in that they no longer restore the stack before being called
    - some seem to have `udf 0x8002` at the end as a trap as well
    - c++'s `[[noreturn]]` doesn't seem to support function pointers so we'll use `__attribute__((__noreturn__))`
  - `nn::ro::detail::ArchData` sdk version was updated to match