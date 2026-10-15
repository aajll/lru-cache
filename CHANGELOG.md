# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- CI cross-builds the library with TI C2000 CGT 22.6.2 and 25.11.1 under the COFF and EABI object formats and both lookup strategies, with warnings as errors.

### Fixed

- `lru-cache.pc` links `-llru-cache`, puts the `lru_cache/` header directory on the include path, and carries the lookup strategy define, so a consumer builds with `pkg-config --cflags --libs lru-cache`. CI installs each strategy and links a consumer through `pkg-config`.
- `-Dlookup_strategy=linear` builds the linear strategy, and `lru_cache_dep` carries the strategy define to consumers. The test suite builds a library per strategy, so the linear lookup is tested.
- `LRU_CACHE_HASH_TABLE_SIZE` above 65535 fails the build with a `_Static_assert`. The hash index is `uint16_t`, so slots from 65536 up were never used.

### Changed

- `LRU_CACHE_HASH_TABLE_SIZE` defaults to 37 and `LRU_CACHE_MAX_PROBES` to 8, so a default cache holds 16 entries and rejects 0.24% of new keys once full, where 17 and 4 held 10 sequential keys and rejected 59%. `sizeof(lru_cache_t)` grows from 236 to 276 octets; code that sizes or places the structure needs the new value.
- `lru_cache.h` defines `LRU_CACHE_VERSION_MAJOR`, `_MINOR`, `_PATCH`, `LRU_CACHE_VERSION` and a new `LRU_CACHE_VERSION_NUMBER` (`long`, `MAJOR * 10000 + MINOR * 100 + PATCH`), so a copied-in header carries its version. Meson fails to configure when the header and `project(version:)` disagree, and the build needs Meson 0.57 or later.
- Insertion, eviction and the list helpers drop checks that the cache invariants make unreachable, so every branch is tested; CI gates line and branch coverage at 100 %.
- CI runs a MISRA C:2023 analysis with `misch`, for the hash and linear lookup strategies, against baselines in `analysis/baseline/` that record the existing findings, and checks formatting with the clang-format release pinned in `scripts/format.sh`.
- CI installs Meson, Ninja and gcovr with `uv` instead of `pip`, and the Meson files and tests carry SPDX licence headers.
- The README is a short landing page, and the integration guide, API reference and design record are in `docs/`. The design record gives the measured rate at which the hash strategy rejects new keys for several table sizes. A `lru_cache docs examples` test compiles every C example in the README, `CONTRIBUTING.md` and `docs/`.
- Added changelog, contributor guide, security policy, SPDX headers, cleaned ignore rules, sanitiser-enabled CI, and a standard coverage gate.

### Removed

- `lru_cache_version.h`. `lru_cache.h` defines the version macros.

## [1.1.6] - 2026-03-21

### Fixed

- Guarded the hash lookup test helper with the lookup strategy define to avoid unused-function warnings in non-hash builds.

## [1.0.0] - 2026-02-25

### Added

- Static-memory deterministic LRU cache with pointer-free index lists, configurable hash or linear lookup, fixed capacity, and bounded execution time.
- Workflow status badge to README.
