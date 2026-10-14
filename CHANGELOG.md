# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- CI cross-builds the library with TI C2000 CGT 22.6.2 and 25.11.1 under the COFF and EABI object formats and both lookup strategies, with warnings as errors.

### Fixed

- `-Dlookup_strategy=linear` builds the linear strategy, and `lru_cache_dep` carries the strategy define to consumers. The test suite builds a library per strategy, so the linear lookup is tested.

### Changed

- Insertion, eviction and the list helpers drop checks that the cache invariants make unreachable, so every branch is tested; CI gates line and branch coverage at 100 %.
- Added changelog, contributor guide, security policy, SPDX headers, cleaned ignore rules, sanitiser-enabled CI, and a standard coverage gate.

## [1.1.6] - 2026-03-21

### Fixed

- Guarded the hash lookup test helper with the lookup strategy define to avoid unused-function warnings in non-hash builds.

## [1.0.0] - 2026-02-25

### Added

- Static-memory deterministic LRU cache with pointer-free index lists, configurable hash or linear lookup, fixed capacity, and bounded execution time.
- Workflow status badge to README.
