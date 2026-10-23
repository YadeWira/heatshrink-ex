# heatshrink Changes By Release

## v0.6.0 - 2026-10-23

### New

Optional frame header for the command line tool (`-f` when encoding). It is
eight bytes:

    0..3  'H' 'S' 'H' 'R'      magic
    4     version (1)
    5     flags (0 in version 1, reserved)
    6     window_sz2 (4..15)
    7     lookahead_sz2 (3..14)

The payload stays the raw heatshrink bit stream, byte compatible with upstream
0.4.x. Decoding detects the header automatically and takes `-w`/`-l` from it,
so `heatshrink -d file.hs` is enough; `-r` forces raw interpretation, `-f` on
decode requires a frame, and a command line `-w`/`-l` that contradicts the
header is an error instead of a silent mis-decode.

Raw output is still the default, so existing files, scripts and embedded
pipelines are unaffected.

## v0.5.0 - 2026-10-22

### Platform support

Added a portable CMake build (Linux x86/x86_64/ARM/AArch64, Windows
x86/x86_64 with MSVC or MinGW-w64, macOS) that installs the libraries and
the headers and exports a `find_package(heatshrink CONFIG)` package.

The command line tool no longer requires `<getopt.h>`, `<err.h>`,
`<unistd.h>`, `ssize_t` or variable length arrays, so it builds with MSVC
and on Windows 7 or newer. Standard input/output are switched to binary
mode on Windows and the option parser is built in (bundled flags such as
`-dv` and attached values such as `-w11` are supported, plus `-V`).

Removed flexible array members from the public structs so that the library
also compiles with MSVC in C mode; allocations now use `offsetof`.

Added `extern "C"` guards to the public headers, so the library can be used
from C++.

### API Changes

`heatshrink_encoder_sink()` and `heatshrink_decoder_sink()` now take a
`const uint8_t *` input buffer.

The internal index (`struct hs_index`) uses `uint16_t` positions and a
`size_t` size, which is required for 15-bit windows to work.

`heatshrink_config.h` moved from `src/` to `include/`, so that installed
headers can find it and out-of-tree builds do not need `-Isrc`.


### Bug Fixes

Fixed fallthrough in getopt 'h'.

Fixed a crash (out-of-bounds read in `find_longest_match`) and a possible
infinite loop when finishing an encoder that had no input, e.g. when
compressing an empty file. (Upstream issue #57, PR #87.)

Fixed 15-bit windows (`-w 15`): the encoder used to emit literals only
(9/8 expansion) and loop forever once the input buffer filled up, because
positions in the search index overflowed a signed 16-bit integer. Indexing
now uses unsigned 16-bit positions in both the indexed and the brute force
search path. (Upstream issues #55 and #86, PR #88.)

Fixed `heatshrink_decoder_poll()` returning `HSDR_POLL_MORE` when the
output buffer was exactly as large as the decoded data, which made callers
that retry on `MORE` spin forever. (Upstream issue #74.)

Fixed a NULL dereference in the test suite: two tests allocated a decoder
with lookahead bits equal to the window bits, which always fails, and then
called `free()` on the NULL pointer. (Upstream issue #98.)


### Other Improvements

Added `CHANGELOG.md`.

Added comment to help explain build error when attempting to build the
command line interface for embedded systems without getopt.h/err.h.

Added `#ifndef`s around defaults for static configuration variables
(`HEATSHRINK_STATIC_INPUT_BUFFER_SIZE`, `HEATSHRINK_STATIC_WINDOW_BITS`,
`HEATSHRINK_STATIC_LOOKAHEAD_BITS`, `HEATSHRINK_DEBUGGING_LOGS`,
`HEATSHRINK_USE_INDEX`). Thanks @ajaybhargav.

Use constant rather than bare int 4096 for buffer size.

Fixed fallthrough compiler warning.

Move repo into `include/`, `src/`, `test/`, and build in `build/`.

Update theft tests to match interface changes in theft.

greatest.h: Update vendored copy to 1.5.0.

Fix warnings with new version of greatest.

Fixed warnings for unreachable code on ARMCC. (Thanks @sport-climber.)

Fixed various typos, reported by @UnePierre.

Added checks and error messages when the CLI is given invalid
parameters for the window or lookahead bits.

Addressed a bunch of warnings from clang's `-Weverything`, which
mostly had to do with mixing up numeric types.


## v0.4.1 - 2015-11-01

### API Changes

None.


### Bug Fixes

Update inaccurate assertion. (Only active when logging.)


### Other Improvements

Fix minor typo.

benchmark: Scrape file size from ls | awk -- stat & du args are less portable.

Update contact info.


## v0.4.0 - 2015-06-07

### API Changes

The limits for the window and lookahead size options have changed. The
minimum lookahead bits option (-l) is now 3 (previously, 2), and the
maximum is the window bits - 1 (previously, equal to the window bits).

Change `get_bits()` to `uint16_t`.


### Bug Fixes

Fix search when window size equals lookahead.

Correct encoder state machine graph.


### Other Improvements

Fix signed/unsigned comparisons.

Use consistently signed vars during index construction.

Fix debug output and asserts.

Vary window and lookahead sizes in theft tests, add a regression test.
Thanks to @unixdj for finding this bug and providing very detailed
info about how to reproduce it!

Simplify break even point calculation a bit further.

Clean up / dead code elimination.

Misc test cleanup.

Add compression benchmarking script.

Initial work on a CONTRIBUTING.md.

README: Add details on example usage and recommended configuration defaults.

Consistently use "decompress", not "uncompress".


## v0.3.2 - 2015-01-03

### API Changes

None.


### Bug Fixes

Fix handling of `O_BINARY` on Windows.


### Other Improvements

Add optional property-based tests, using theft (if available).

Fix compilation on MinGW and other platforms without err.h

Print all status messages on stderr.

Fix missing #include for getopt.h.

Complete overhaul of Makefile: both static & dynamic variant static libs.



## v0.3.1 - 2014-06-25

### API Changes

None.


### Bug Fixes

Fix possible state yielding MORE from finish(), but 0 bytes from poll().


### Other Improvements

Improve non-indexed compression performance.

When indexed, only check matches that can beat the current best match.

Only check for matches that can be improvements.

Update heatshrink_common.h filename in README.md to reflect rename.

Add usage details, homepage to `heatshrink -h` message.

CLI: note that compression is the default.x


## v0.3.0 - 2013-12-28

### API Changes

Update non-indexed matching to use an `int16_t` pos.

Update index type to `int16_t`.

Decrease `HEATSHRINK_MAX_WINDOW_BITS` to 15.


### Bug Fixes

None.


### Other Improvements

Remove unused static library target from Makefile.

Cast `write(2)`'s -1 to (size_t) to avoid sign mismatch warning.

Code style cleanup: _always_ brace `if`s, extract NO_BITS constant.

Eliminate redundant comparison, index ensures first byte matches.

Improve throughput of `find_longest_match` via several improvements
to the indexing strategy.

If calling push_bits w/ 8 bits at beginning of new byte, just copy byte over whole.


## v0.2.1 - 2013-12-18

### API Changes

None.


### Bug Fixes

None.


### Other Improvements

Renamed `heatshrink.h` to `heatshrink_common.h`, to make it more
clearly separate from the `heatshrink.c` CLI interface.

Don't use assert unless `HEATSHRINK_DEBUGGING_LOGS` is set.

Fix some (potentially) unused variable and numeric type warnings.

Add warnings from `-Wextra`.

Add braces around statements hanging off `if`s that may compile out to no-ops.

Fix usage string ("[-v]" not "[v]").

Change from BSD-3 License to ISC License.

Reduce default `static_input_buffer_size`.

Update decoder state machine diagram to reflect MSB/LSB split.



## v0.2.0 - 2013-05-12

### API Changes

Change `uint16_t` to `size_t` for user-controlled buffer sizes.


### Bug Fixes

When index or count are > 8 bits long, ensure suspend/resume is correct.


### Other Improvements

Consistentcy: log to stderr for both enc and dec.

Tighten bounds for lookahead and window size.
Since they're only stored as `uint16_t`s, disallow overly large sizes.


## v0.1.0 - 2013-03-13

Initial public release.
