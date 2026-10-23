# heatshrink-ex vendor drop

Version: @VERSION@ (tag `v@VERSION@`)
Upstream: heatshrink 0.4.1 by Scott Vokes (https://github.com/atomicobject/heatshrink)
Fork: https://github.com/YadeWira/heatshrink-ex
License: ISC (see LICENSE)

These six files are the complete library. Build them together with your
project:

    heatshrink_encoder.c  heatshrink_encoder.h
    heatshrink_decoder.c  heatshrink_decoder.h
    heatshrink_common.h   heatshrink_config.h

There are no dependencies beyond the C standard library and no build system is
required; compile the two `.c` files with the same flags as the rest of your
project.

## Configuration

`heatshrink_config.h` defaults to `HEATSHRINK_DYNAMIC_ALLOC 1` and
`HEATSHRINK_USE_INDEX 1`. All of its settings are wrapped in `#ifndef`, so
they can also be overridden from the compiler command line.

## API

Unchanged from 0.4.1 for the five entry points:

    heatshrink_encoder_alloc(window_sz2, lookahead_sz2)
    heatshrink_encoder_sink(hse, in_buf, size, &input_size)
    heatshrink_encoder_poll(hse, out_buf, out_buf_size, &output_size)
    heatshrink_encoder_finish(hse)
    heatshrink_encoder_free(hse)

and the decoder equivalents. `sink()` takes a `const uint8_t *` (source
compatible for normal callers) and `free()` accepts NULL.

## Compatibility with 0.4.1 output

For every window size below 15 (`-w 4` .. `-w 14`) and every valid lookahead
value, @VERSION@ emits byte-identical streams to 0.4.1: verified on 198
(file, -w, -l) combinations over text, mixed and random data (files up to
654 KB), zero differences. Existing archives stay valid.

At `-w 15` the output differs because 0.4.1 is broken there (it emits literals
only and hangs on inputs of 32 KiB or more); @VERSION@ actually compresses.
Streams produced at `-w 15` decode with the 0.4.1 decoder.

## Fixed

* empty input no longer crashes (upstream #57)
* 15-bit windows work (upstream #55, #86)
* `decoder_poll()` no longer reports MORE when the output buffer is exactly
  the size of the decoded data (upstream #74)
* `free(NULL)` is safe
