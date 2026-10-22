# API reference

`lru_cache.h` declares the whole interface. The examples in this document compile together as one file after `#include "lru_cache.h"`.

## Conventions

- The cache is an `lru_cache_t` that the caller owns. It may be static, on the stack, or embedded in another structure; at the default configuration it is 276 bytes on x86-64, so a static instance is usual on small stacks. No function allocates or blocks.
- The members of `lru_cache_t` are visible because the caller allocates it, but treat them as private: read and change the cache only through the functions.
- Keys and values are `uint32_t`. Any key except `LRU_CACHE_INVALID_KEY` (`0xFFFFFFFFU`), which marks an empty node, may be stored.
- Every pointer argument is checked. A NULL argument never dereferences.
- No function is safe to call concurrently with another on the same cache, including `lru_cache_get`, which reorders the recency list. [design.md](design.md#concurrency) gives the contract.

## Initialisation

```c
void lru_cache_init(lru_cache_t *cache_ptr, uint16_t capacity);
```

`lru_cache_init` empties the cache and sets the number of entries it holds before evicting. A capacity above `LRU_CACHE_MAX_ENTRIES` is reduced to `LRU_CACHE_MAX_ENTRIES`, and 0 is raised to 1. A NULL `cache_ptr` does nothing. Call it before any other function on the cache; calling it again discards every entry.

## Lookup

```c
bool lru_cache_get(lru_cache_t *cache_ptr, uint32_t key, uint32_t *out_value);
```

`lru_cache_get` returns true if `key` is in the cache, writes its value to `*out_value`, and makes it the most recently used entry. It returns false, without writing `*out_value` or changing the cache, when the key is absent, when `cache_ptr` or `out_value` is NULL, or when `key` is `LRU_CACHE_INVALID_KEY`.

## Insertion

```c
bool lru_cache_put(lru_cache_t *cache_ptr, uint32_t key, uint32_t value);
```

`lru_cache_put` stores `value` under `key` and makes it the most recently used entry:

- If the key is present, its value is replaced. This always succeeds.
- If the key is absent and the cache holds fewer entries than its capacity, a new entry is added.
- If the key is absent and the cache is full, the least recently used entry is evicted and its node reused.

It returns false, and changes nothing, when `cache_ptr` is NULL, when `key` is `LRU_CACHE_INVALID_KEY`, or, under the hash strategy, when no hash table slot within `LRU_CACHE_MAX_PROBES` of the key's hash can take it. That last case happens for a new key even when the cache is not full, and how often depends on how full the table is; [design.md](design.md#probe-window-and-rejected-inserts) gives measured rates. Under the linear strategy a put with valid arguments always succeeds.

<!-- snippet: body -->
```c
static lru_cache_t readings;
uint32_t value = 0U;

lru_cache_init(&readings, 8U);
if (!lru_cache_put(&readings, 42U, 1234U)) {
        /* not cached: the next lookup of 42 misses */
}
if (lru_cache_get(&readings, 42U, &value)) {
        /* value is 1234 */
}
```

## Constants

<!-- snippet: skip -->
```c
#define LRU_CACHE_LOOKUP_HASH    0U
#define LRU_CACHE_LOOKUP_LINEAR  1U
#define LRU_CACHE_INVALID_KEY    0xFFFFFFFFU
#define LRU_CACHE_HASH_TOMBSTONE ((int16_t)-2)
```

The two strategy identifiers are the values of `LRU_CACHE_LOOKUP_STRATEGY`. `LRU_CACHE_INVALID_KEY` is the reserved key. `LRU_CACHE_HASH_TOMBSTONE` marks a hash table slot whose entry was evicted; it is internal to the library and appears in the header only because the table is part of `lru_cache_t`.

## Configuration macros

Each macro is defined in `lru_cache.h` behind an `#ifndef` guard and is overridden with a `-D` flag. The two that size `lru_cache_t` must match between the library and its callers; [integration.md](integration.md#configuration) explains how.

| Macro | Default | Effect |
| --- | --- | --- |
| `LRU_CACHE_MAX_ENTRIES` | `16U` | Number of nodes, and the largest capacity, from 1 to 32767. Layout. |
| `LRU_CACHE_HASH_TABLE_SIZE` | `37U` | Number of hash table slots, from `LRU_CACHE_MAX_ENTRIES` to 65535. The table is allocated under both strategies. Layout. |
| `LRU_CACHE_MAX_PROBES` | `8U` | Slots a hash lookup or insert examines, from 1 to `LRU_CACHE_HASH_TABLE_SIZE`. Hash strategy only. |
| `LRU_CACHE_LOOKUP_STRATEGY` | `LRU_CACHE_LOOKUP_HASH` | `LRU_CACHE_LOOKUP_HASH` or `LRU_CACHE_LOOKUP_LINEAR`. |

The limits are `_Static_assert`s in the header, so an out-of-range value stops every translation unit that includes it. The defaults hold a full cache and reject 0.24% of new keys once it is full. A larger `LRU_CACHE_MAX_ENTRIES` needs a larger table, and the rate rises with the entry count; [design.md](design.md#probe-window-and-rejected-inserts) gives measured rates.

## Recipes

### Get or compute

Look a key up, and on a miss compute the value and cache it. The result is correct whether or not the put succeeds; a rejected put only means the next lookup computes again.

```c
uint32_t compute_checksum(uint32_t block);

uint32_t
cached_checksum(lru_cache_t *cache, uint32_t block)
{
        uint32_t sum = 0U;

        if (!lru_cache_get(cache, block, &sum)) {
                sum = compute_checksum(block);
                (void)lru_cache_put(cache, block, sum);
        }
        return sum;
}
```

### A RAM budget

Check the size of the structure where the cache is declared, so a configuration change that outgrows its budget stops the build:

```c
_Static_assert(sizeof(lru_cache_t) <= 512U,
               "LRU cache exceeds its RAM budget");
```

`sizeof` counts 16-bit words on a target where `CHAR_BIT` is 16, so express the budget in the target's units. [design.md](design.md#footprint) gives the size formula.

## Version

`lru_cache.h` defines the release it belongs to, so a copied-in header still identifies itself:

<!-- snippet: skip -->
```c
#define LRU_CACHE_VERSION_MAJOR 1
#define LRU_CACHE_VERSION_MINOR 2
#define LRU_CACHE_VERSION_PATCH 0
#define LRU_CACHE_VERSION "1.2.0"
```

`LRU_CACHE_VERSION_NUMBER` is `MAJOR * 10000 + MINOR * 100 + PATCH` as a `long`, so it holds any version where `int` is 16 bits. Compare it in the preprocessor to require a minimum release:

```c
#if LRU_CACHE_VERSION_NUMBER < 10200L
#error "lru_cache 1.2.0 or later is required"
#endif
```
