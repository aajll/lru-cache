# lru-cache

[![CI](https://github.com/aajll/lru-cache/actions/workflows/ci.yml/badge.svg)](https://github.com/aajll/lru-cache/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

**A C11 least recently used cache of 32-bit keys and values in one caller-owned structure, with no heap and every operation bounded by a compile-time constant.**

## Quick start

Copy `include/lru_cache.h` and `src/lru_cache.c` into your project. For a Meson subproject or an installed package, see the [integration guide](docs/integration.md).

```c
#include <inttypes.h>
#include <stdio.h>

#include "lru_cache.h"

static lru_cache_t cache;

int
main(void)
{
        uint32_t value = 0U;

        lru_cache_init(&cache, 2U);
        (void)lru_cache_put(&cache, 101U, 500U);
        (void)lru_cache_put(&cache, 102U, 600U);

        /* A hit makes 101 the most recently used: prints "101 -> 500". */
        if (lru_cache_get(&cache, 101U, &value)) {
                printf("101 -> %" PRIu32 "\n", value);
        }

        /* The cache is full, so 102, the least recently used, is evicted. */
        (void)lru_cache_put(&cache, 103U, 700U);

        /* Prints "102 cached: 0". */
        printf("102 cached: %d\n", (int)lru_cache_get(&cache, 102U, &value));
        return 0;
}
```

## Description

The cache is one `lru_cache_t` that the caller declares. It holds up to `LRU_CACHE_MAX_ENTRIES` nodes in a fixed array, linked into a recency list by 16-bit indices rather than pointers, so its size is known at compile time and the structure can be copied or relocated.

- `get`, which promotes a hit to most recently used, and `put`, which updates a key in place or evicts the least recently used entry when the cache is full
- A hash table with bounded linear probing, or a linear scan, chosen at build time; each lookup is bounded by `LRU_CACHE_MAX_PROBES` or the capacity
- A runtime capacity from 1 to `LRU_CACHE_MAX_ENTRIES`, so instances of one build can hold different numbers of entries
- A failed insert leaves the cache unchanged
- Tests of both strategies under ASan and UBSan with 100% line and branch coverage, and MISRA C:2023 analysis with [`misch`](https://pypi.org/project/misch/)

## Validated toolchains

| Toolchain               | Target                        | Validation                                           |
| ----------------------- | ----------------------------- | ---------------------------------------------------- |
| GCC                     | x86_64 Linux                  | All tests, both lookup strategies                    |
| DSP vendor C11 compiler | 16-bit MAU (`CHAR_BIT == 16`) | Library compile, both strategies, two object formats |

Target linking and execution belong to the consuming firmware. See [cross builds](docs/integration.md#cross-builds).

## Documentation

- [Integration guide](docs/integration.md) — copy-in, Meson subproject, installed package, configuration, building, and cross builds
- [API reference](docs/api.md) — initialisation, lookup, insertion, the configuration macros, and recipes
- [Design](docs/design.md) — the index list, the lookup strategies, tombstones, insertion and eviction, the footprint, concurrency, failure behaviour, and verification
- [Changelog](CHANGELOG.md) — release history

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before you open a pull request. Report security issues as described in [SECURITY.md](.github/SECURITY.md).

## License

MIT. See [LICENSE](LICENSE).
