#ifndef HEATSHRINK_CONFIG_H
#define HEATSHRINK_CONFIG_H

/* Should functionality assuming dynamic allocation be used? */
#ifndef HEATSHRINK_DYNAMIC_ALLOC
#define HEATSHRINK_DYNAMIC_ALLOC 1
#endif

#if HEATSHRINK_DYNAMIC_ALLOC
    /* Optional replacement of malloc/free */
    #define HEATSHRINK_MALLOC(SZ) malloc(SZ)
    #define HEATSHRINK_FREE(P, SZ) free(P)
#else
    /* Required parameters for static configuration */
    #ifndef HEATSHRINK_STATIC_INPUT_BUFFER_SIZE
    #define HEATSHRINK_STATIC_INPUT_BUFFER_SIZE 32
    #endif

    #ifndef HEATSHRINK_STATIC_WINDOW_BITS
    #define HEATSHRINK_STATIC_WINDOW_BITS 8
    #endif

    #ifndef HEATSHRINK_STATIC_LOOKAHEAD_BITS
    #define HEATSHRINK_STATIC_LOOKAHEAD_BITS 4
    #endif
#endif

/* Turn on logging for debugging. */
#ifndef HEATSHRINK_DEBUGGING_LOGS
#define HEATSHRINK_DEBUGGING_LOGS 0
#endif

/* Use indexing for faster compression. (This requires additional space.) */
#ifndef HEATSHRINK_USE_INDEX
#define HEATSHRINK_USE_INDEX 1
#endif

/* Key the search index by two bytes instead of one. A one byte match is never
 * worth a back-reference at any supported window/lookahead setting, so this
 * only stops examining candidates that cannot win: the compressed output is
 * unchanged. Costs 2^HEATSHRINK_HASH_BITS uint16_t entries of stack while the
 * index is built (1 KB with the default 9). Set to 0 for the original one-byte
 * chains, which is the cheaper option on very small targets. */
#ifndef HEATSHRINK_HASH_BITS
#define HEATSHRINK_HASH_BITS 9
#endif
#if HEATSHRINK_HASH_BITS < 0 || HEATSHRINK_HASH_BITS > 16
#error "HEATSHRINK_HASH_BITS must be between 0 and 16"
#endif
#if HEATSHRINK_HASH_BITS > 0
#define HEATSHRINK_HASH_SIZE (1u << HEATSHRINK_HASH_BITS)
#endif

#endif
