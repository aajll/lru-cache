# lru-cache

[![CI](https://github.com/aajll/lru-cache/actions/workflows/ci.yml/badge.svg)](https://github.com/aajll/lru-cache/actions/workflows/ci.yml)

A static-memory, deterministic Least Recently Used (LRU) cache in C for
safety-critical embedded systems.

## Features

- **No dynamic memory** — fixed-size static arrays; no `malloc` / `free`.
- **Pointer-free internals** — the doubly-linked list uses integer indices,
  satisfying MISRA C:2012 Rule 18.4 (no pointer arithmetic).
- **Configurable lookup** — switch between a hash table (`O(1)` average) and
  linear scan (`O(n)`) at compile time with a single macro.
- **Deterministic WCET** — every loop is bounded by a compile-time constant
  (`LRU_CACHE_MAX_ENTRIES` or `LRU_CACHE_MAX_PROBES`).

## Installation

### Copy-in (recommended for embedded targets)

Copy two files into your project tree — no build system required:

```
include/lru_cache.h
src/lru_cache.c
```

Place them in the same directory, then include the header:

```c
#include "lru_cache.h"
```

### Meson subproject

Add this repo as a wrap dependency or subproject. The library exposes a
`lru_cache_dep` dependency object that carries the correct include path:

```meson
lru_cache_dep = dependency('lru-cache', fallback : ['lru-cache', 'lru_cache_dep'])
```

## Quick Start

```c
#include <inttypes.h>
#include <stdio.h>
#include "lru_cache.h"

int main(void)
{
        lru_cache_t cache;
        uint32_t    val;

        lru_cache_init(&cache, 4U);

        lru_cache_put(&cache, 101U, 500U);
        lru_cache_put(&cache, 102U, 600U);

        if (lru_cache_get(&cache, 101U, &val)) {
                printf("key 101 -> %" PRIu32 "\n", val);
        }

        return 0;
}
```

## Configuration

All macros can be overridden before including the header or passed as
`-D` flags on the compiler command line.

| Macro | Description | Default |
|---|---|---|
| `LRU_CACHE_MAX_ENTRIES` | Maximum items stored. | `16U` |
| `LRU_CACHE_HASH_TABLE_SIZE` | Hash table slot count; a prime `>= MAX_ENTRIES` reduces collisions. | `17U` |
| `LRU_CACHE_MAX_PROBES` | Maximum linear-probe steps during hash collision resolution. Bounds WCET. | `4U` |
| `LRU_CACHE_LOOKUP_STRATEGY` | `LRU_CACHE_LOOKUP_HASH` or `LRU_CACHE_LOOKUP_LINEAR`. | `LRU_CACHE_LOOKUP_HASH` |

### Choosing a lookup strategy

```c
/* Simpler code path, guaranteed WCET = N — preferred for certification */
#define LRU_CACHE_LOOKUP_STRATEGY LRU_CACHE_LOOKUP_LINEAR

/* Hash table with bounded probing — lower average-case cost */
#define LRU_CACHE_LOOKUP_STRATEGY LRU_CACHE_LOOKUP_HASH
```

With the Meson build system, set the strategy at configure time:

```sh
meson setup build -Dlookup_strategy=linear
```

## Building

```sh
# Library only (release)
meson setup build --buildtype=release
meson compile -C build

# With unit tests
meson setup build --buildtype=debug -Dbuild_tests=true
meson compile -C build
meson test -C build
```

## Notes

| Topic | Note |
|---|---|
| **Memory** | All storage is static. Verify `LRU_CACHE_MAX_ENTRIES` fits your stack / BSS budget. |
| **Thread safety** | Not thread-safe. Protect each cache instance with an external mutex. |
| **Reserved key** | `0xFFFFFFFFU` (`LRU_CACHE_INVALID_KEY`) is reserved as an empty-slot sentinel. Do not use it as a user key. |
| **WCET (hash)** | Worst-case probe count is `LRU_CACHE_MAX_PROBES`. Verify this satisfies your timing budget. |
| **WCET (linear)** | Worst-case scan count equals `capacity`. Simpler to bound formally. |
