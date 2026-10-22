/**
 * SPDX-License-Identifier: MIT
 *
 * @file: lru_cache.h
 *
 * @brief
 *    Fixed-size LRU cache with configurable lookup.
 *
 * This module provides a fixed-size LRU cache suitable for safety-critical
 * systems. Lookup strategy (Hash vs Linear) is selected at compile time.
 *
 * Configuration macros (define before including or pass via -D):
 *   LRU_CACHE_MAX_ENTRIES      - Max items stored (default 16).
 *   LRU_CACHE_HASH_TABLE_SIZE  - Hash table slots; prime >= MAX_ENTRIES
 *                                (default 37).
 *   LRU_CACHE_MAX_PROBES       - Collision probe limit, bounds WCET
 *                                (default 8).
 *   LRU_CACHE_LOOKUP_STRATEGY  - LRU_CACHE_LOOKUP_HASH or
 *                                LRU_CACHE_LOOKUP_LINEAR.
 */

#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <stdbool.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Version
 * ------------------------------------------------------------------------- */

/**
 * @name Version
 * @brief Release of this header, for diagnostics and @c #if checks.
 *
 * @details
 *    @c LRU_CACHE_VERSION_NUMBER is @c MAJOR*10000 @c + @c MINOR*100
 *    @c + @c PATCH with type @c long, so it cannot overflow where @c int is
 *    16 bits. The Meson build fails to configure when these values differ
 *    from the project version.
 * @{
 */
/* cppcheck-suppress-begin misra-c2012-2.5 ; @deviation Public version macros
 * for consumer diagnostics and #if checks; the library does not use them. */
#define LRU_CACHE_VERSION_MAJOR 1
#define LRU_CACHE_VERSION_MINOR 2
#define LRU_CACHE_VERSION_PATCH 0
#define LRU_CACHE_VERSION       "1.2.0"
#define LRU_CACHE_VERSION_NUMBER                                               \
        ((LRU_CACHE_VERSION_MAJOR * 10000L) + (LRU_CACHE_VERSION_MINOR * 100L) \
         + LRU_CACHE_VERSION_PATCH)
/* cppcheck-suppress-end misra-c2012-2.5 ; @deviation As above. */
/** @} */

/* -------------------------------------------------------------------------
 * Lookup strategy identifiers
 * ------------------------------------------------------------------------- */

/** @brief Use hash table lookup (O(1) avg, WCET bounded by MAX_PROBES). */
#define LRU_CACHE_LOOKUP_HASH   0U

/** @brief Use linear scan lookup (O(n), simpler, guaranteed WCET = N). */
#define LRU_CACHE_LOOKUP_LINEAR 1U

/* -------------------------------------------------------------------------
 * Compile-time configuration
 * ------------------------------------------------------------------------- */

/**
 * @brief Maximum number of entries in the cache.
 *
 * From 1 to 32767 under both strategies, because node indices are int16_t.
 * For hash strategy: choose a value whose next prime fits the table size.
 */
#ifndef LRU_CACHE_MAX_ENTRIES
#define LRU_CACHE_MAX_ENTRIES 16U
#endif

/**
 * @brief Hash table size.
 *
 * Must be >= LRU_CACHE_MAX_ENTRIES and <= 65535, because hash indices are
 * uint16_t. A prime number reduces collisions, and about twice
 * LRU_CACHE_MAX_ENTRIES keeps rejected inserts rare.
 * Only relevant when LRU_CACHE_LOOKUP_STRATEGY == LRU_CACHE_LOOKUP_HASH.
 */
#ifndef LRU_CACHE_HASH_TABLE_SIZE
#define LRU_CACHE_HASH_TABLE_SIZE 37U
#endif

/**
 * @brief Maximum linear-probe steps during hash collision resolution.
 *
 * Bounds the worst-case execution time for hash lookups and insertions.
 * Must satisfy: LRU_CACHE_MAX_PROBES <= LRU_CACHE_HASH_TABLE_SIZE.
 */
#ifndef LRU_CACHE_MAX_PROBES
#define LRU_CACHE_MAX_PROBES 8U
#endif

/**
 * @brief Lookup strategy selection.
 *
 * Default: LRU_CACHE_LOOKUP_HASH.
 * Override by defining before including this header or via -D on the
 * compiler command line (e.g.
 * -DLRU_CACHE_LOOKUP_STRATEGY=LRU_CACHE_LOOKUP_LINEAR).
 */
#ifndef LRU_CACHE_LOOKUP_STRATEGY
#define LRU_CACHE_LOOKUP_STRATEGY LRU_CACHE_LOOKUP_HASH
#endif

/* -------------------------------------------------------------------------
 * Compile-time configuration validation
 * ------------------------------------------------------------------------- */

_Static_assert(LRU_CACHE_MAX_ENTRIES <= 32767U,
               "LRU_CACHE_MAX_ENTRIES must be <= INT16_MAX (32767)");

_Static_assert(LRU_CACHE_HASH_TABLE_SIZE >= LRU_CACHE_MAX_ENTRIES,
               "LRU_CACHE_HASH_TABLE_SIZE must be >= LRU_CACHE_MAX_ENTRIES");

_Static_assert(LRU_CACHE_HASH_TABLE_SIZE <= 65535U,
               "LRU_CACHE_HASH_TABLE_SIZE must be <= UINT16_MAX (65535)");

_Static_assert(LRU_CACHE_MAX_PROBES <= LRU_CACHE_HASH_TABLE_SIZE,
               "LRU_CACHE_MAX_PROBES must be <= LRU_CACHE_HASH_TABLE_SIZE");

_Static_assert(LRU_CACHE_MAX_ENTRIES > 0U, "LRU_CACHE_MAX_ENTRIES must be > 0");

_Static_assert(LRU_CACHE_MAX_PROBES > 0U, "LRU_CACHE_MAX_PROBES must be > 0");

/**
 * @brief Reserved sentinel for an empty/invalid key slot.
 *
 * Do NOT use 0xFFFFFFFFU as a user key.
 */
#define LRU_CACHE_INVALID_KEY    0xFFFFFFFFU

/**
 * @brief Tombstone marker for deleted hash table slots.
 *
 * Used to maintain probe chain integrity when entries are evicted.
 * Must be distinct from empty slot marker (-1) and valid indices (>= 0).
 * Value is -2, which fits in int16_t but is outside the valid range.
 */
#define LRU_CACHE_HASH_TOMBSTONE ((int16_t)-2)

/* -------------------------------------------------------------------------
 * Data types
 * ------------------------------------------------------------------------- */

/**
 * @brief Single node in the doubly-linked LRU list.
 *
 * Indices are used instead of pointers to avoid pointer arithmetic
 * violations (MISRA Rule 18.4) and to keep the structure relocatable.
 */
typedef struct {
        uint32_t key;     /**< Unique key identifier. */
        uint32_t value;   /**< Associated data value. */
        int16_t prev_idx; /**< Index of previous node, -1 if none. */
        int16_t next_idx; /**< Index of next node, -1 if none. */
} lru_cache_node_t;

/**
 * @brief Main LRU Cache structure.
 *
 * Declare as a global or static variable to avoid stack pressure on
 * embedded targets.
 */
typedef struct {
        uint16_t capacity; /**< Configured item limit. */
        uint16_t size;     /**< Current number of stored items. */
        int16_t head_idx;  /**< MRU node index, -1 when empty. */
        int16_t tail_idx;  /**< LRU node index, -1 when empty. */
        lru_cache_node_t nodes[LRU_CACHE_MAX_ENTRIES]; /**< Node storage. */
        int16_t
            hash_table[LRU_CACHE_HASH_TABLE_SIZE]; /**< key -> node index. */
} lru_cache_t;

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/**
 * @brief Initialise the LRU cache.
 *
 * Resets all internal state. Must be called before any other function.
 *
 * @param cache_ptr  Pointer to an lru_cache_t instance (must not be NULL).
 * @param capacity   Maximum items to hold; clamped to LRU_CACHE_MAX_ENTRIES.
 *                   Passing 0 is treated as 1.
 */
void lru_cache_init(lru_cache_t *cache_ptr, uint16_t capacity);

/**
 * @brief Look up a key and return its associated value.
 *
 * On a hit the entry is promoted to MRU position.
 *
 * @param cache_ptr  Pointer to an initialised lru_cache_t (must not be NULL).
 * @param key        Key to search for (must not equal LRU_CACHE_INVALID_KEY).
 * @param[out] out_value  Receives the stored value on success (must not be
 * NULL).
 * @return true on cache hit, false on miss or invalid arguments.
 */
bool lru_cache_get(lru_cache_t *cache_ptr, uint32_t key, uint32_t *out_value);

/**
 * @brief Insert or update a key-value pair.
 *
 * If the key already exists its value is updated and it is promoted to MRU.
 * If the cache is full the LRU entry is evicted before insertion.
 *
 * @param cache_ptr  Pointer to an initialised lru_cache_t (must not be NULL).
 * @param key        Key to insert/update (must not equal
 * LRU_CACHE_INVALID_KEY).
 * @param value      Value to store.
 * @return true on success; false on invalid arguments, or under the hash
 *         strategy when no slot within LRU_CACHE_MAX_PROBES of the key's
 *         hash can take a new key.
 */
bool lru_cache_put(lru_cache_t *cache_ptr, uint32_t key, uint32_t value);

#endif /* LRU_CACHE_H */
