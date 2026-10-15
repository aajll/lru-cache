# AGENTS.md

---

## 1) Project-specific instructions

**Project:** `lru-cache`
**Primary goal:** A C11 least recently used cache of 32-bit keys and values in one caller-owned structure, with a hash or linear lookup chosen at build time. No heap, no system calls, no global state, and every operation bounded by a compile-time constant.

### 1.1 Essential commands

#### Configure and build (library only)

```sh
meson setup build --wipe --buildtype=release -Dbuild_tests=false
meson compile -C build
```

#### Configure, build, and run unit tests

```sh
meson setup build --wipe --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose
```

The suite builds a copy of the library for each lookup strategy and runs `tests/test_lru_cache.c` against both, whatever `lookup_strategy` is set to.

#### MISRA C:2023, hash and linear strategies (reads `build/compile_commands.json`)

```sh
env -u MISRA_RULE_TEXTS misch run --baseline
env -u MISRA_RULE_TEXTS misch run --baseline --profile linear
misch deviations
```

#### Formatting (pinned clang-format through `uvx`)

```sh
scripts/format.sh          # check, as CI does
scripts/format.sh --fix    # rewrite in place
```

Coverage commands are in `CONTRIBUTING.md`.

### 1.2 Notes

- `include/lru_cache.h` defines the version macros, and `meson setup` fails when they differ from `project(version:)`, so a release updates both.
- The build produces only a static library, installed as `liblru-cache.a` with headers under `lru_cache/`, and `lru-cache.pc`. The Meson options are `build_tests` and `lookup_strategy`; `lru_cache_dep` carries the strategy define to consumers.
- `LRU_CACHE_MAX_ENTRIES` and `LRU_CACHE_HASH_TABLE_SIZE` change the layout of `lru_cache_t` and must match between the library and its callers. `LRU_CACHE_MAX_PROBES` is read only by `src/lru_cache.c`.

---

## 2) Non-negotiable design invariants

- **Bounded time.** Every loop is bounded by `LRU_CACHE_MAX_ENTRIES`, `LRU_CACHE_HASH_TABLE_SIZE`, `LRU_CACHE_MAX_PROBES` or the capacity. No recursion.
- **Fail clean.** `lru_cache_put` finds every slot and node it needs before writing anything. A put that returns `false`, and every lookup that misses, leaves the cache unchanged.
- **Probe window.** Under the hash strategy, every key sits within `LRU_CACHE_MAX_PROBES` slots of its hash. An evicted slot becomes a tombstone, and only tombstones that end a probe sequence return to empty.
- **Index list.** Nodes link by `int16_t` index with -1 for none, head is most recently used and tail least. Nodes `[0, size)` are in use while the cache fills; there is no free list.
- Not thread-safe; `lru_cache_get` writes. No heap, no system calls, no blocking, no global state.

---

## 3) CI / source of truth

- CI definitions live in `.github/workflows/ci.yml`.
- Prefer running the same commands locally as CI runs (see §1.1 above).
- Install Python tools (Meson, Ninja, gcovr, misch) with `uv tool install`, never `pip`; CI does the same through `astral-sh/setup-uv`. CI installs `cppcheck` for the MISRA job from Homebrew because Ubuntu's 2.13 reports different findings.
- `misch run --baseline` must pass against `analysis/baseline/` for both strategies. Never regenerate a baseline to admit a new finding. Run it with `MISRA_RULE_TEXTS` unset, as CI does.
- The CI jobs are: tests under ASan/UBSan, release builds for both strategies, a 100 percent line and branch coverage gate, a TI C2000 library build (CGT 22.6.2 and 25.11.1, COFF and EABI, both strategies, warnings as errors), `clang-format`, and `misch` for both strategies.

---

## 4) Docs / commit conventions

### Documentation layout

- `README.md` is a short landing page (under 100 lines). Put detail in `docs/`, never in the README; link to the owning file instead.
- `docs/integration.md` owns requirements, install methods, configuration, building, and cross builds.
- `docs/api.md` owns the API reference, the constants, the configuration macros and recipes. Change it together with the header; keep signatures in fenced `c` blocks and describe them in prose, not function tables. Check every value an example states against a build.
- `docs/design.md` is the single authoritative design record: the recency list, lookup strategies, tombstones, the measured rejection rates, insertion and eviction, worst-case cost, footprint, concurrency, failure behaviour, MISRA position, and verification.
- `CONTRIBUTING.md` documents the contributor workflow; `CHANGELOG.md` holds release history (Keep a Changelog); `.github/SECURITY.md` covers vulnerability reporting.
- Code blocks in `README.md`, `CONTRIBUTING.md`, and `docs/` are compiled by the `lru_cache docs examples` test (`tests/docs/check_snippets.py`). Each document is compiled as one source file: a `c` fence is file-level code, `<!-- snippet: body -->` wraps statements, and `<!-- snippet: skip -->` excludes pseudo-code. Do not use `skip` to silence a broken example.
- Prose uses British spelling. Do not hard-wrap Markdown: write one line per paragraph. The 80-column limit applies to C source, not Markdown.

### Commits

- Use **Conventional Commits** format when asked to commit.
- Keep commits focused; explain *why* in the message body.

---

## 5) C style expectations

### Build & configuration

- Use the Meson build system. Do not introduce CMake, Make, or other systems.
- Update `meson.build` and `tests/meson.build` when adding or removing files.
- The public header is `include/lru_cache.h`. Every configuration macro stays behind an `#ifndef` guard with a `_Static_assert` on its range.

### Formatting

- `.clang-format` is present and **mandatory**. Run `scripts/format.sh --fix` before committing. It runs the clang-format release pinned in the script through `uvx`. Do not use a system `clang-format`: another release can format the same code differently.
- Do not reformat unrelated code.
- Key settings: 8-space indent, `BreakBeforeBraces: Linux`, column limit 80.

### Style & correctness

- Match conventions in the existing files (indentation, braces, naming).
- Every public identifier uses the `lru_cache_` or `LRU_CACHE_` prefix. Keep the public header minimal; do not expose internal helpers.
- Validate pointer arguments at every public API boundary and return `false`; no `errno`.
- Use `uint32_t`, `uint16_t`, `int16_t` and `bool`, never plain `int` for fixed-width fields, so the code builds where `CHAR_BIT` is 16.
- A change to the layout of `lru_cache_t` needs a `CHANGELOG.md` entry.
- Public symbols carry Doxygen with the full contract: preconditions and return semantics.

### MISRA deviations

- Prefer compliant code; never restructure only to hide a finding. Deviate at the point of use with `/* cppcheck-suppress misra-c2012-<rule> ; @deviation <rationale> */`, or project-wide in `analysis/deviations/misra-deviations.txt` for a house-style rule.

### Comment placement (Doxygen)

- Inline trailing annotations (`/**< ... */`) on `enum`/`struct` members are allowed only when the resulting line fits the 80-column limit.
- If any member's annotation would overrun, move **all** of that aggregate's member docs into a single structured Doxygen block above the type, as an `@details` list of `- ::SYMBOL  description` entries.
- Never mix inline and block forms within one aggregate, and never leave a trailing comment that clang-format would wrap onto a second line.
- After editing, run `scripts/format.sh` and an 80-column scan.

### Testing

- Run `meson test -C build` after every change; both strategy tests must pass.
- Unit tests live in `tests/test_lru_cache.c`; hash-only tests sit behind the strategy define. Do not add Unity or another external framework.
- Test each guard with the input that reaches it: NULL arguments, `LRU_CACHE_INVALID_KEY`, out-of-range capacities, a full probe window, colliding keys.
- Add a test case for each bug fix and each new feature.
- Keep `src/` and `include/` at the coverage gate (100 percent line and branch).
