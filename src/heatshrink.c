/*
 * heatshrink command line tool.
 *
 * This file is deliberately portable C99: it must build with GCC/Clang on
 * Linux, ARM and macOS, and with MSVC and MinGW-w64 on Windows (x86 and
 * x86_64, Windows 7 or newer). It therefore avoids <getopt.h>, <err.h>,
 * <unistd.h>, ssize_t, variable length arrays and other non-portable
 * constructs. Only the C standard library and the heatshrink library are
 * used.
 *
 * The bit stream produced and consumed here is identical to the one used by
 * heatshrink 0.4.x, so files are interchangeable with the upstream tool and
 * with embedded implementations.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>

#include "heatshrink_encoder.h"
#include "heatshrink_decoder.h"

#if defined(_WIN32)
#  include <io.h>
#  include <fcntl.h>
#  define HS_SET_BINARY(fp) _setmode(_fileno(fp), _O_BINARY)
#else
#  define HS_SET_BINARY(fp) ((void)0)
#endif

#define DEF_WINDOW_SZ2 11
#define DEF_LOOKAHEAD_SZ2 4
#define DEF_DECODER_INPUT_BUFFER_SIZE 256
#define DEF_BUFFER_SIZE (64 * 1024)

/* Portable size_t format: MSVC before 2015 does not know %zu, and the
 * msvcrt.dll stdio implementation used by MinGW does not either. */
#define HS_SIZE_FMT "%llu"
#define HS_SIZE(v) ((unsigned long long)(v))

static const int version_major = HEATSHRINK_VERSION_MAJOR;
static const int version_minor = HEATSHRINK_VERSION_MINOR;
static const int version_patch = HEATSHRINK_VERSION_PATCH;
static const char author[] = HEATSHRINK_AUTHOR;
static const char url[] = HEATSHRINK_URL;

typedef enum { IO_READ, IO_WRITE } IO_mode;
typedef enum { OP_ENC, OP_DEC } Operation;

typedef struct {
    FILE *fp;
    IO_mode mode;
    int owns_fp;                /* close the stream when done? */
    size_t cap;                 /* buffer capacity */
    size_t fill;                /* valid bytes in buf */
    size_t pos;                 /* read position within buf */
    size_t total;               /* bytes read or written, for reporting */
    uint8_t buf[1];             /* actually `cap` bytes */
} io_handle;

typedef struct {
    uint8_t window_sz2;
    uint8_t lookahead_sz2;
    size_t decoder_input_buffer_size;
    size_t buffer_size;
    int verbose;
    int out_to_stdout;          /* report goes to stderr in that case */
    Operation cmd;
    const char *in_fname;
    const char *out_fname;
    io_handle *in;
    io_handle *out;
} config;

static void report(config *cfg);

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fputs("heatshrink: ", stderr);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

static void die_errno(const char *what) {
    int e = errno;
    die("%s: %s", what, strerror(e));
}

static void usage(FILE *stream) {
    fprintf(stream, "heatshrink version %d.%d.%d by %s\n",
        version_major, version_minor, version_patch, author);
    fprintf(stream, "Home page: %s\n\n", url);
    fprintf(stream,
        "Usage:\n"
        "  heatshrink [-h] [-V] [-e|-d] [-v] [-w SIZE] [-l BITS] [IN_FILE] [OUT_FILE]\n"
        "\n"
        "heatshrink compresses or decompresses byte streams using LZSS, and is\n"
        "designed especially for embedded, low-memory, and/or hard real-time\n"
        "systems.\n"
        "\n"
        " -h        print help\n"
        " -V        print version and exit\n"
        " -e        encode (compress, default)\n"
        " -d        decode (decompress)\n"
        " -v        verbose (print input & output sizes, compression ratio, etc.)\n"
        "\n"
        " -w SIZE   Base-2 log of LZSS sliding window size (default: %d)\n"
        "\n"
        "    A larger value allows searches a larger history of the data for repeated\n"
        "    patterns, potentially compressing more effectively, but will use\n"
        "    more memory and processing time.\n"
        "    Recommended default: -w 8 (embedded systems), -w 10 (elsewhere)\n"
        "\n"
        " -l BITS   Number of bits used for back-reference lengths (default: %d)\n"
        "\n"
        "    A larger value allows longer substitutions, but since all\n"
        "    back-references must use -w + -l bits, larger -w or -l can be\n"
        "    counterproductive if most patterns are small and/or local.\n"
        "\n"
        " -i SIZE   Decoder input buffer size in bytes (default: %d)\n"
        "\n"
        " If IN_FILE or OUT_FILE are unspecified, they will default to\n"
        " \"-\" for standard input and standard output, respectively.\n"
        "\n"
        " NOTE: compression settings are not stored in the output stream. When\n"
        " decoding, use the same -w and -l values that were used to encode.\n",
        DEF_WINDOW_SZ2, DEF_LOOKAHEAD_SZ2, DEF_DECODER_INPUT_BUFFER_SIZE);
}

static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (p == NULL) { die("out of memory allocating " HS_SIZE_FMT " bytes", HS_SIZE(size)); }
    return p;
}

/* Open an IO handle. Exits on error. */
static io_handle *handle_open(const char *fname, IO_mode m, size_t buf_sz) {
    if (buf_sz == 0) { buf_sz = 1; }

    io_handle *io = xmalloc(sizeof(*io) + buf_sz - 1);
    memset(io, 0, sizeof(*io) + buf_sz - 1);
    io->mode = m;
    io->cap = buf_sz;
    io->owns_fp = 0;

    if (m == IO_READ) {
        if (strcmp(fname, "-") == 0) {
            io->fp = stdin;
            HS_SET_BINARY(io->fp);
        } else {
            io->fp = fopen(fname, "rb");
            if (io->fp == NULL) { die_errno(fname); }
            io->owns_fp = 1;
        }
    } else {
        if (strcmp(fname, "-") == 0) {
            io->fp = stdout;
            HS_SET_BINARY(io->fp);
        } else {
            io->fp = fopen(fname, "wb");
            if (io->fp == NULL) { die_errno(fname); }
            io->owns_fp = 1;
        }
    }

    return io;
}

/* Read SIZE bytes from an IO handle and return a pointer to the content.
 * BUF is valid until the next call for this handle. Returns false on error. */
static bool handle_read(io_handle *io, size_t size, uint8_t **buf,
        size_t *read_size) {
    if ((io == NULL) || (buf == NULL) || (read_size == NULL)) { return false; }
    if (io->mode != IO_READ) { return false; }
    if (size > io->cap) { return false; }

    size_t rem = io->fill - io->pos;
    if (rem < size) {           /* compact and refill */
        if (rem > 0) { memmove(io->buf, io->buf + io->pos, rem); }
        io->fill = rem;
        io->pos = 0;

        size_t got = fread(io->buf + io->fill, 1, io->cap - io->fill, io->fp);
        if ((got == 0) && ferror(io->fp)) { return false; }
        io->fill += got;
        io->total += got;
        rem = io->fill;
    }

    *buf = io->buf + io->pos;
    *read_size = (rem < size) ? rem : size;
    return true;
}

/* Drop the oldest SIZE bytes from the read buffer. Returns false on error. */
static bool handle_drop(io_handle *io, size_t size) {
    if ((io == NULL) || (io->mode != IO_READ)) { return false; }
    if (size > io->fill - io->pos) { return false; }
    io->pos += size;
    if (io->pos == io->fill) {
        io->pos = 0;
        io->fill = 0;
    }
    return true;
}

static bool handle_flush(io_handle *io) {
    size_t off = 0;
    while (off < io->fill) {
        size_t n = fwrite(io->buf + off, 1, io->fill - off, io->fp);
        if (n == 0) {
            if (ferror(io->fp)) { return false; }
            break;              /* should not happen without an error */
        }
        off += n;
        io->total += n;
    }
    io->fill = 0;
    return true;
}

/* Sink SIZE bytes from DATA into the io handle. Returns false on error. */
static bool handle_sink(io_handle *io, const uint8_t *data, size_t size,
        size_t *sink_size) {
    if ((io == NULL) || (sink_size == NULL) || (data == NULL)) { return false; }
    if (io->mode != IO_WRITE) { return false; }
    if (size > io->cap) { return false; }

    if (io->fill + size > io->cap) {
        if (!handle_flush(io)) { return false; }
    }
    memcpy(io->buf + io->fill, data, size);
    io->fill += size;
    *sink_size = size;
    return true;
}

static bool handle_close(io_handle *io) {
    bool ok = true;
    if (io == NULL) { return false; }

    if (io->mode == IO_WRITE) {
        ok = handle_flush(io);
    }
    if (io->fp != NULL) {
        if (fflush(io->fp) != 0) { ok = false; }
        if (io->owns_fp && (fclose(io->fp) != 0)) { ok = false; }
        io->fp = NULL;
    }
    return ok;
}

static void handle_free(io_handle *io) {
    if (io != NULL) { free(io); }
}

static int encoder_sink_read(config *cfg, heatshrink_encoder *hse,
        uint8_t *data, size_t data_sz, uint8_t *out_buf) {
    size_t out_sz = cfg->buffer_size;
    size_t sink_sz = 0;
    size_t poll_sz = 0;
    io_handle *out = cfg->out;

    size_t sunk = 0;
    do {
        if (data_sz > 0) {
            HSE_sink_res sres = heatshrink_encoder_sink(hse, &data[sunk],
                data_sz - sunk, &sink_sz);
            if (sres < 0) { die("encoder sink failed (%d)", (int)sres); }
            sunk += sink_sz;
        }

        HSE_poll_res pres;
        do {
            pres = heatshrink_encoder_poll(hse, out_buf, out_sz, &poll_sz);
            if (pres < 0) { die("encoder poll failed (%d)", (int)pres); }
            size_t sunk_bytes = 0;
            if (!handle_sink(out, out_buf, poll_sz, &sunk_bytes)) {
                die_errno("write");
            }
        } while (pres == HSER_POLL_MORE);

        if (poll_sz == 0 && data_sz == 0) {
            HSE_finish_res fres = heatshrink_encoder_finish(hse);
            if (fres < 0) { die("encoder finish failed (%d)", (int)fres); }
            if (fres == HSER_FINISH_DONE) { return 1; }
        }
    } while (sunk < data_sz);
    return 0;
}

static int encode(config *cfg) {
    uint8_t window_sz2 = cfg->window_sz2;
    size_t window_sz = (size_t)1 << window_sz2;
    uint8_t *out_buf = xmalloc(cfg->buffer_size);

    heatshrink_encoder *hse = heatshrink_encoder_alloc(window_sz2,
        cfg->lookahead_sz2);
    if (hse == NULL) { die("failed to init encoder: bad settings"); }

    io_handle *in = cfg->in;

    /* Process input until end of stream */
    while (1) {
        uint8_t *input = NULL;
        size_t read_sz = 0;
        if (!handle_read(in, window_sz, &input, &read_sz) || input == NULL) {
            die_errno("read");
        }

        /* Pass read to encoder and check if input is fully processed. */
        if (encoder_sink_read(cfg, hse, input, read_sz, out_buf)) { break; }

        if (!handle_drop(in, read_sz)) { die("internal error: bad drop"); }
    }

    heatshrink_encoder_free(hse);
    free(out_buf);

    bool ok = handle_close(cfg->in);
    ok = handle_close(cfg->out) && ok;
    if (!ok) { die_errno("close"); }
    if (cfg->verbose) { report(cfg); }
    handle_free(cfg->in);
    handle_free(cfg->out);
    return 0;
}

static int decoder_sink_read(config *cfg, heatshrink_decoder *hsd,
        uint8_t *data, size_t data_sz, uint8_t *out_buf) {
    io_handle *out = cfg->out;
    size_t sink_sz = 0;
    size_t poll_sz = 0;
    size_t out_sz = cfg->buffer_size;

    size_t sunk = 0;
    do {
        if (data_sz > 0) {
            HSD_sink_res sres = heatshrink_decoder_sink(hsd, &data[sunk],
                data_sz - sunk, &sink_sz);
            if (sres < 0) { die("decoder sink failed (%d)", (int)sres); }
            sunk += sink_sz;
        }

        HSD_poll_res pres;
        do {
            pres = heatshrink_decoder_poll(hsd, out_buf, out_sz, &poll_sz);
            if (pres < 0) { die("decoder poll failed (%d)", (int)pres); }
            size_t sink_bytes = 0;
            if (!handle_sink(out, out_buf, poll_sz, &sink_bytes)) {
                die_errno("write");
            }
        } while (pres == HSDR_POLL_MORE);

        if ((data_sz == 0) && (poll_sz == 0)) {
            HSD_finish_res fres = heatshrink_decoder_finish(hsd);
            if (fres < 0) { die("decoder finish failed (%d)", (int)fres); }
            if (fres == HSDR_FINISH_DONE) { return 1; }
        }
    } while (sunk < data_sz);

    return 0;
}

static int decode(config *cfg) {
    uint8_t window_sz2 = cfg->window_sz2;
    size_t window_sz = (size_t)1 << window_sz2;
    uint8_t *out_buf = xmalloc(cfg->buffer_size);

    heatshrink_decoder *hsd = heatshrink_decoder_alloc(
        (uint16_t)cfg->decoder_input_buffer_size, window_sz2,
        cfg->lookahead_sz2);
    if (hsd == NULL) { die("failed to init decoder: bad settings"); }

    io_handle *in = cfg->in;

    /* Process input until end of stream */
    while (1) {
        uint8_t *input = NULL;
        size_t read_sz = 0;
        if (!handle_read(in, window_sz, &input, &read_sz) || input == NULL) {
            die_errno("read");
        }
        if (read_sz == 0) {
            HSD_finish_res fres = heatshrink_decoder_finish(hsd);
            if (fres < 0) { die("decoder finish failed (%d)", (int)fres); }
            if (fres == HSDR_FINISH_DONE) { break; }
        } else {
            if (decoder_sink_read(cfg, hsd, input, read_sz, out_buf)) { break; }
            if (!handle_drop(in, read_sz)) { die("internal error: bad drop"); }
        }
    }

    heatshrink_decoder_free(hsd);
    free(out_buf);

    bool ok = handle_close(cfg->in);
    ok = handle_close(cfg->out) && ok;
    if (!ok) { die_errno("close"); }
    if (cfg->verbose) { report(cfg); }
    handle_free(cfg->in);
    handle_free(cfg->out);
    return 0;
}

static void report(config *cfg) {
    size_t inb = cfg->in->total;
    size_t outb = cfg->out->total;
    double ratio = (inb > 0) ? 100.0 - (100.0 * (double)outb) / (double)inb : 0.0;
    FILE *stream = cfg->out_to_stdout ? stderr : stdout;

    fprintf(stream, "%s %0.2f %%\t " HS_SIZE_FMT " -> " HS_SIZE_FMT
        " (-w %u -l %u)\n",
        cfg->in_fname, ratio, HS_SIZE(inb), HS_SIZE(outb),
        cfg->window_sz2, cfg->lookahead_sz2);
}

static bool parse_uint(const char *opt, const char *arg, long min, long max,
        long *out) {
    char *end = NULL;
    long v;

    errno = 0;
    v = strtol(arg, &end, 10);
    if ((errno != 0) || (end == arg) || (*end != '\0') || (v < min) || (v > max)) {
        fprintf(stderr, "heatshrink: invalid value for %s: '%s'"
            " (expected %ld..%ld)\n", opt, arg, min, max);
        return false;
    }
    *out = v;
    return true;
}

static void proc_args(config *cfg, int argc, char **argv) {
    cfg->window_sz2 = DEF_WINDOW_SZ2;
    cfg->lookahead_sz2 = DEF_LOOKAHEAD_SZ2;
    cfg->buffer_size = DEF_BUFFER_SIZE;
    cfg->decoder_input_buffer_size = DEF_DECODER_INPUT_BUFFER_SIZE;
    cfg->cmd = OP_ENC;
    cfg->verbose = 0;
    cfg->in_fname = "-";
    cfg->out_fname = "-";

    int positional = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if ((arg[0] != '-') || (strcmp(arg, "-") == 0)) {
            /* positional argument, including "-" for stdin/stdout */
            if (positional == 0) {
                cfg->in_fname = arg;
            } else if (positional == 1) {
                cfg->out_fname = arg;
            } else {
                fprintf(stderr, "heatshrink: too many file arguments\n");
                exit(EXIT_FAILURE);
            }
            positional++;
            continue;
        }
        if (strcmp(arg, "--") == 0) {
            for (i++; i < argc; i++) {
                if (positional == 0) {
                    cfg->in_fname = argv[i];
                } else if (positional == 1) {
                    cfg->out_fname = argv[i];
                } else {
                    fprintf(stderr, "heatshrink: too many file arguments\n");
                    exit(EXIT_FAILURE);
                }
                positional++;
            }
            break;
        }

        /* Short options, possibly bundled (-dv) or with an attached value
         * (-w11). */
        for (const char *p = arg + 1; *p != '\0'; p++) {
            char opt = *p;
            const char *value = NULL;

            switch (opt) {
            case 'h':
                usage(stdout);
                exit(EXIT_SUCCESS);
            case 'V':
                printf("heatshrink %d.%d.%d\n", version_major, version_minor,
                    version_patch);
                exit(EXIT_SUCCESS);
            case 'e':
                cfg->cmd = OP_ENC;
                continue;
            case 'd':
                cfg->cmd = OP_DEC;
                continue;
            case 'v':
                cfg->verbose++;
                continue;
            case 'w':
            case 'l':
            case 'i':
                break;
            default:
                fprintf(stderr, "heatshrink: unknown option '-%c'\n", opt);
                usage(stderr);
                exit(EXIT_FAILURE);
            }

            /* Options taking a value: use the rest of this argument, or the
             * next one. */
            if (p[1] != '\0') {
                value = p + 1;
            } else if (i + 1 < argc) {
                value = argv[++i];
            } else {
                fprintf(stderr, "heatshrink: option '-%c' requires a value\n", opt);
                exit(EXIT_FAILURE);
            }

            long v = 0;
            if (opt == 'w') {
                if (!parse_uint("-w", value, 0, 255, &v)) { exit(EXIT_FAILURE); }
                cfg->window_sz2 = (uint8_t)v;
            } else if (opt == 'l') {
                if (!parse_uint("-l", value, 0, 255, &v)) { exit(EXIT_FAILURE); }
                cfg->lookahead_sz2 = (uint8_t)v;
            } else {            /* 'i' */
                if (!parse_uint("-i", value, 1, 0xFFFF, &v)) { exit(EXIT_FAILURE); }
                cfg->decoder_input_buffer_size = (size_t)v;
            }
            break;              /* value consumed the rest of this argument */
        }
    }

    if ((cfg->window_sz2 < HEATSHRINK_MIN_WINDOW_BITS)
        || (cfg->window_sz2 > HEATSHRINK_MAX_WINDOW_BITS)) {
        fprintf(stderr, "heatshrink: invalid window bits: %u (expected %d..%d)\n",
            cfg->window_sz2, HEATSHRINK_MIN_WINDOW_BITS,
            HEATSHRINK_MAX_WINDOW_BITS);
        exit(EXIT_FAILURE);
    }
    if ((cfg->lookahead_sz2 < HEATSHRINK_MIN_LOOKAHEAD_BITS)
        || (cfg->lookahead_sz2 >= cfg->window_sz2)) {
        fprintf(stderr, "heatshrink: invalid lookahead bits: %u "
            "(expected %d..%u)\n", cfg->lookahead_sz2,
            HEATSHRINK_MIN_LOOKAHEAD_BITS, (unsigned)(cfg->window_sz2 - 1));
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv) {
    config cfg;
    memset(&cfg, 0, sizeof(cfg));
    proc_args(&cfg, argc, argv);

    if ((strcmp(cfg.in_fname, cfg.out_fname) == 0)
        && (strcmp(cfg.in_fname, "-") != 0)) {
        fprintf(stderr, "heatshrink: refusing to overwrite file '%s' with itself\n",
            cfg.in_fname);
        exit(EXIT_FAILURE);
    }

    cfg.in = handle_open(cfg.in_fname, IO_READ, cfg.buffer_size);
    cfg.out = handle_open(cfg.out_fname, IO_WRITE, cfg.buffer_size);
    cfg.out_to_stdout = (cfg.out->fp == stdout);

    /* Keep the streams in binary mode on Windows, so that CR/LF translation
     * and Ctrl-Z do not corrupt the data. */
    HS_SET_BINARY(cfg.in->fp);
    HS_SET_BINARY(cfg.out->fp);

    if (cfg.cmd == OP_DEC) {
        return decode(&cfg);
    }
    return encode(&cfg);
}
