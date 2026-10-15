# Integration guide

This guide shows how to add `lru-cache` to a project, configure it, build it, and run its tests. The API is described in [api.md](api.md), and the data structure and its guarantees in [design.md](design.md).

## Requirements

- A C11 compiler with `_Static_assert`, and a target where `CHAR_BIT` is 8 or 16.
- `<stdint.h>` with `uint32_t`, `uint16_t` and `int16_t`, `<stdbool.h>` and `<stddef.h>`. The library calls no standard library function.
- For the tests: Meson, Ninja and Python 3.

## Copy-in

Copy two files into your source tree:

```
include/lru_cache.h
src/lru_cache.c
```

Compile `lru_cache.c` with the directory holding `lru_cache.h` on the include path, then include the header:

```c
#include "lru_cache.h"
```

`lru_cache.h` defines the version macros described in [api.md](api.md#version).

## Meson subproject

Put the repository in `subprojects/lru-cache`, as a checkout or through a wrap file, then declare the dependency:

```meson
lru_cache_dep = dependency('lru-cache', fallback: ['lru-cache', 'lru_cache_dep'])
```

`dependency('lru-cache')` finds an installed package through `pkg-config`, and otherwise falls back to the subproject. The subproject registers itself under that name, so other subprojects in the same build that ask for `lru-cache` get the same library. `lru_cache_dep` links the static library, adds `include/`, and carries the lookup strategy define, so the caller compiles the header with the strategy the library was built with.

| Option | Default | Effect |
| --- | --- | --- |
| `lookup_strategy` | `hash` | `hash` or `linear`; see [design.md](design.md#lookup-strategies). |
| `build_tests` | `false` | Builds the unit tests for both strategies and the documentation example test. |

Set a subproject option from the consuming build with the subproject prefix:

```sh
meson setup build -Dlru-cache:lookup_strategy=linear
```

## Installed package

```sh
meson setup build --buildtype=release -Dbuild_tests=false --prefix=/usr/local
meson compile -C build
meson install -C build
cc app.c -I/usr/local/include/lru_cache -L/usr/local/lib -llru-cache -o app
```

The install contains the static archive `liblru-cache.a`, the headers in a `lru_cache/` subdirectory, and `lru-cache.pc`. The `pkg-config` file carries the include root and the version but no link flags, so name the library on the link line, and either put `lru_cache/` on the include path as above or include the header by its subdirectory:

<!-- snippet: skip -->
```c
#include <lru_cache/lru_cache.h>
```

An installed library keeps the strategy and the configuration it was built with.

## Configuration

Every configuration macro in `lru_cache.h` sits behind an `#ifndef` guard, so a `-D` definition replaces the default. The macros are described in [api.md](api.md#configuration-macros).

`LRU_CACHE_MAX_ENTRIES` and `LRU_CACHE_HASH_TABLE_SIZE` set the size of the arrays in `lru_cache_t`, which the caller allocates and the library indexes. The library and every file that includes the header must be compiled with the same values. In a copy-in build, put the definitions on the compiler command line for the whole project. In a Meson build, the top-level `c_args` also apply to subprojects, so one setting reaches both:

```sh
meson setup build "-Dc_args=-DLRU_CACHE_MAX_ENTRIES=32U -DLRU_CACHE_HASH_TABLE_SIZE=37U"
```

`LRU_CACHE_MAX_PROBES` and `LRU_CACHE_LOOKUP_STRATEGY` are read only by `src/lru_cache.c`. In a Meson build, select the strategy with the `lookup_strategy` option rather than `c_args`, because the build already defines `LRU_CACHE_LOOKUP_STRATEGY` from that option.

## Build and test

```sh
# Library only
meson setup build --buildtype=release -Dbuild_tests=false
meson compile -C build

# All tests under ASan and UBSan
meson setup build --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose
```

The test suite builds its own copy of the library for each strategy, so one configuration runs both whatever `lookup_strategy` is set to. Coverage, formatting and MISRA commands are in [CONTRIBUTING.md](../CONTRIBUTING.md).

## Cross builds

The consuming firmware owns its cross file, linker command file, and memory placement. No cross file ships with this project.

CI compiles the library with warnings as errors using two releases of a vendor C11 compiler for a 16-bit-MAU DSP target, under both of its object formats and for both lookup strategies. This covers the source dialect and type assumptions only; linking and execution remain checks for the consumer.

On such a target `CHAR_BIT` is 16 and `sizeof` counts 16-bit words. [design.md](design.md#footprint) gives the size of the structure there.
