# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- CI cross-builds the library with TI C2000 CGT 22.6.2 and 25.11.1 under the COFF and EABI object formats and both lookup strategies, with warnings as errors.

### Fixed

- `-Dlookup_strategy=linear` builds the linear strategy, and `lru_cache_dep` carries the strategy define to consumers. The test suite builds a library per strategy, so the linear lookup is tested.

### Changed

- `lru_cache.h` defines `LRU_CACHE_VERSION_MAJOR`, `_MINOR`, `_PATCH`, `LRU_CACHE_VERSION` and a new `LRU_CACHE_VERSION_NUMBER` (`long`, `MAJOR * 10000 + MINOR * 100 + PATCH`), so a copied-in header carries its version. `lru_cache_version.h` is no longer generated; it is installed as a deprecated header that includes `lru_cache.h`. Meson fails to configure when the header and `project(version:)` disagree, and the build needs Meson 0.57 or later.
- Insertion, eviction and the list helpers drop checks that the cache invariants make unreachable, so every branch is tested; CI gates line and branch coverage at 100 %.
- CI runs a MISRA C:2023 analysis with `misch`, for the hash and linear lookup strategies, against baselines in `analysis/baseline/` that record the existing findings, and checks formatting with the clang-format release pinned in `scripts/format.sh`.
- CI installs Meson, Ninja and gcovr with `uv` instead of `pip`, and the Meson files and tests carry SPDX licence headers.
- The README is a short landing page, and the integration guide, API reference and design record are in `docs/`. The design record gives the measured rate at which the hash strategy rejects new keys for several table sizes. A `lru_cache docs examples` test compiles every C example in the README, `CONTRIBUTING.md` and `docs/`.
- Added changelog, contributor guide, security policy, SPDX headers, cleaned ignore rules, sanitiser-enabled CI, and a standard coverage gate.

## [1.1.6] - 2026-03-21

### Fixed

- Guarded the hash lookup test helper with the lookup strategy define to avoid unused-function warnings in non-hash builds.

## [1.0.0] - 2026-02-25

### Added

- Static-memory deterministic LRU cache with pointer-free index lists, configurable hash or linear lookup, fixed capacity, and bounded execution time.
- Workflow status badge to README.
