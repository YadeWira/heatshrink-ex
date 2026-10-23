# Releasing heatshrink-ex

The version lives in exactly one place: `include/heatshrink_common.h`.
CMake reads it, so there is nothing else to bump.

## Checklist

1. Clean tree, `master` up to date.
2. `ctest` green in at least three flavours: `Release`, `Debug` (`-O0`) and
   `-fsanitize=address,undefined`.
3. CI green on the release commit (13 jobs: Linux x86_64 GCC/Clang, i686,
   shared, debug, sanitizers, macOS, MSVC Win32/x64, MinGW 32/64, AArch64 and
   ARMv7 under QEMU).
4. `./make-release.sh` from a clean tree. It writes to `dist/`:
   * `heatshrink-ex-<version>.tar.gz` (source)
   * `heatshrink-ex-<version>-vendor.tar.gz` (the six library files in `hs/`)
   * `heatshrink-ex-<version>-win32.zip`, `-win64.zip`
   * `SHA256SUMS`
5. Verify the vendor tarball against the tag:
   `git show v<version>:src/... | cmp - hs/heatshrink_encoder.c` for all six
   files (also checked by hand before every release so far).
6. Annotated tag `v<version>`, push it, then create the GitHub release with the
   five assets and release notes.
7. Update the wiki: `Command line tool`, `API`, `Platforms` (verification log),
   `Integration`, `Home` if the feature table changed.

## Library vs CLI releases

Two audiences consume this project:

* **Embedders** (zpaq-std and friends) vendor the six library files and do not
  care about CLI flags.
* **Desktop users** use the command line tool.

Release notes must say which of the two changed. A release is a **library
change** when anything in these six files changes beyond the version line:

    include/heatshrink_common.h      (version line only does not count)
    include/heatshrink_config.h
    include/heatshrink_encoder.h
    include/heatshrink_decoder.h
    src/heatshrink_encoder.c
    src/heatshrink_decoder.c

Otherwise it is a **CLI change** (this is the case for 0.6.0 and 0.7.0:
`-f`/`-c` live entirely in `src/heatshrink.c`).

Embedders such as zpaq-std batch vendor updates: they pick up CLI-only
releases when they next publish something, and expect an explicit heads-up
when the library itself changes. Say so in the release notes and in the
message to them.

## Compatibility promises

* Raw output stays byte-identical to upstream heatshrink 0.4.1 for `-w 4`
  .. `-w 14`, and stays the default.
* The frame (`-f`) and its extension (`-c`) are opt-in and versioned; old
  frames keep decoding.
* Frame layout changes need a version bump in the header, never a silent
  reinterpretation.
