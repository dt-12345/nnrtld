# nnrtld

shitty rtld decompilation (well, reimplementation currently bc decompilation is hard)

based on the rtld shipped with SDK version 20.5.6
- the build id seems to vary by game/version, but everything else is identical
- I haven't checked how lld generates the build id, but it likely includes parts of the ELF that aren't included in the NSO

rtld appears to be compiled (or at least linked) alongside the game, so it follows optimization flags from the game

i.e. if a game is compiled with PGO, then rltd is also compiled with PGO

most functions do not match, but I would like to eventually get them there

function names and TU splits are mostly just guesses - some are based on the `rocrt` from [Nintendo OSS](https://support.nintendo.com/jp/oss/index.html)

compiler is clang 14.0.x, some parts of the binary kind of seem like they might have been compiled with PGO, but I'm not sure

but for any other purpose, it should build with any clang version that supports c++20

TODO:
- match functions
- match data sections (.bss is matching, the others need some work)
  - `-Wl,-z now` seems to add both a bind now entry and a flags entry to `.dynamic` but official rtld only has the flags entry
  - check which functions need `.eh_frame` entries
- unified code style

inspired by https://github.com/marysaka/oss-rtld but this does not use any code from there