# Contributing to lru-cache

`lru-cache` is a small C11 least recently used cache in one caller-owned structure. A broken probe sequence or recency link returns a wrong value or loses an entry without any error, so read [design.md](docs/design.md) before you change lookup, insertion or eviction.

## Getting started

Install the Python tools with [uv](https://docs.astral.sh/uv/), as CI does. The MISRA analysis also needs a current `cppcheck`; CI installs it from Homebrew because the 2.13 release in Ubuntu 24.04 reports different findings. The same commands CI runs, locally:

```sh
# Python tools
uv tool install meson
uv tool install ninja
uv tool install gcovr
uv tool install misch

# Configure with tests and sanitisers (CI default); runs both lookup strategies
meson setup build --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose

# Coverage (CI gate is 100% line and branch)
meson setup build_cov --buildtype=debug -Dbuild_tests=true -Db_coverage=true
meson compile -C build_cov && meson test -C build_cov
gcovr --root . --filter 'src/' --filter 'include/' --fail-under-line 100 --fail-under-branch 100 --print-summary build_cov

# MISRA C:2023, hash and linear strategies (reads build/compile_commands.json)
env -u MISRA_RULE_TEXTS misch run --baseline
env -u MISRA_RULE_TEXTS misch run --baseline --profile linear
misch deviations

# Formatting (pinned clang-format through uvx)
scripts/format.sh --fix
```

## Source style

- Formatting is mandatory. Run `scripts/format.sh --fix` before submitting, or `scripts/format.sh` to check without writing, as CI does. The script pins the clang-format release and runs it through `uvx`, so an editor formatter gives the same result only when it runs the same release.
- 8-space indent, Linux brace style, 80-column limit. Match the existing conventions and do not reformat unrelated code.
- The Meson build system is the single source of truth. Update `meson.build` and `tests/meson.build` when you add or remove files. No CMake, no Make, and no other build systems.

## C language rules

- C11 only.
- Use fixed-width types from `<stdint.h>` and `<stdbool.h>`, and never plain `int` for a fixed-width quantity.
- No heap allocation (`malloc`, `free`, VLAs), no recursion, no system calls, and no global state. Every loop is bounded by `LRU_CACHE_MAX_ENTRIES`, `LRU_CACHE_HASH_TABLE_SIZE` or `LRU_CACHE_MAX_PROBES`.
- Validate every pointer argument at the public API boundary and return `false` rather than fault.
- Every public identifier uses the `lru_cache_` or `LRU_CACHE_` prefix.
- New code builds and passes under both lookup strategies, and compiles where `CHAR_BIT` is 16.

## Cache rules

- Link nodes by `int16_t` index, never by pointer, so the structure stays valid when copied.
- Keep `lru_cache_put` in two phases: find every slot and node it needs without writing, then commit with nothing that can fail. A put that returns `false` leaves the cache unchanged.
- Under the hash strategy, every key stays within `LRU_CACHE_MAX_PROBES` slots of its hash, and an evicted slot becomes a tombstone unless nothing probes past it.
- A change to `lru_cache_t` changes the layout callers allocate. Keep it compatible within a release line, and keep the layout macros listed in [the integration guide](docs/integration.md#configuration).

## MISRA C:2023

The library is analysed with `misch` (cppcheck-backed MISRA C:2023 analysis), configured by `misra.toml`, over `include/` and `src/`, once for the hash strategy and once with the `linear` profile. CI fails on any finding that is not in that strategy's baseline in `analysis/baseline/`. Run it from a configured `build/` directory, with `MISRA_RULE_TEXTS` unset, as CI does. If your change introduces a finding:

1. Prefer restructuring the code so the rule is satisfied. Deviate only when compliance would make the code genuinely worse, and never restructure purely to hide a finding from the checker; an honest deviation beats evasion.
2. Suppress at the point of use with `/* cppcheck-suppress misra-c2012-<rule> ; @deviation <rationale> */`, or add a justified project-wide entry to `analysis/deviations/misra-deviations.txt` for a house-style rule.
3. Verify with `misch run --baseline` for both strategies and `misch deviations`, which requires every suppression to carry a rationale, and justify the deviation in the pull request description.

Do not regenerate a baseline to admit a new finding. It changes only after a deliberate review, with `misch baseline`.

## Tests and coverage

- Add a test for every bug fix and every new feature.
- Unit tests live in `tests/test_lru_cache.c`. The suite builds a copy of the library for each lookup strategy and runs the file against both, as `lru_cache / hash strategy` and `lru_cache / linear strategy`; hash-only tests sit behind the strategy define.
- Test each guard with the input that reaches it, including NULL arguments, `LRU_CACHE_INVALID_KEY`, capacities of 0 and above `LRU_CACHE_MAX_ENTRIES`, a full probe window, and keys that collide.
- CI enforces 100 percent line and branch coverage of `src/` and `include/` across both strategies. Every new branch needs a test that fails when the branch is broken.
- The `lru_cache docs examples` test compiles every C code block in `README.md`, `CONTRIBUTING.md`, and `docs/` against the public header, through `tests/docs/check_snippets.py`. A block compiles at file scope by default. On the line directly above the fence, put `<!-- snippet: body -->` for a block of statements, or `<!-- snippet: skip -->` for pseudo-code or a copy of a header definition.

## Documentation

| File | Purpose |
| --- | --- |
| `README.md` | Landing page: quick start, description, validated toolchains, and links. Keep it short. |
| `docs/integration.md` | Requirements, installation, configuration, building, and cross builds. |
| `docs/api.md` | Reference for initialisation, lookup, insertion, the constants, the configuration macros, and recipes. |
| `docs/design.md` | The recency list, lookup strategies, tombstones, insertion and eviction, worst-case cost, the footprint, concurrency, failure behaviour, MISRA position, and verification. |

Update `docs/api.md` and the header Doxygen together when an interface changes, and check every value an example states against a build. Update `docs/design.md` when lookup, insertion, eviction or the verification changes, and measure the rejection table again when the hash or the defaults change. Keep it standalone: describe how the library works, not how it was built or which other projects use it. Write prose in British English, one line per paragraph.

## API stability

- The function signatures and the configuration macros in `lru_cache.h` are stable within a major release line.
- Which entry an eviction removes is observable behaviour. Changing it or a signature is a breaking change and needs a major release.
- The members of `lru_cache_t` are private, but its size is not: callers allocate it, so a layout change needs a `CHANGELOG.md` entry.

## Commits

Use Conventional Commits:

- `feat: ...` new feature
- `fix: ...` bug fix
- `docs: ...` documentation only
- `test: ...` test-only changes
- `chore: ...` build, CI, release work
- `refactor: ...` a change that neither fixes a bug nor adds a feature

Keep the subject under about 70 characters. Use the body to explain why the change is needed.

## Pull requests

- Open an issue first for non-trivial changes, and always for a change to the hash, the probe rules or eviction order, so the design can be agreed before implementation.
- Keep pull requests focused: one feature or one fix.
- All CI checks must pass: tests under ASan and UBSan, release builds for both strategies, the coverage gate, the TI C2000 build, clang-format, and MISRA analysis against the baselines in `analysis/baseline/`.
- Add a `CHANGELOG.md` entry under `## [Unreleased]` for a user-visible change.
- Flag any new MISRA deviation, and any change to the layout of `lru_cache_t`, in the description.

## When in doubt

Open an issue. The library is small, but a cache that quietly returns a stale or wrong value is worse than no cache.
