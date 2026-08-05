# nnrtld

shitty rtld decompilation (well, reimplementation currently bc decompilation is hard)

based on the rtld shipped with SDK version 20.5.6

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