# Contributing to lru-cache

lru-cache is a small C library providing a static-memory, deterministic
Least Recently Used (LRU) cache. It is designed to be safe to drop into
embedded firmware and audited environments.

## Getting started

The same commands CI runs, locally:

```sh
# Configure with tests + sanitisers (CI default)
meson setup build --buildtype=debug -Dbuild_tests=true \
                  -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose

# Coverage (CI gate is 80% line + 70% branch)
meson setup build_cov --buildtype=debug -Dbuild_tests=true \
                      -Db_coverage=true
meson compile -C build_cov && meson test -C build_cov
gcovr --root . --filter 'src/' --filter 'include/' --print-summary
```

## Source style

- `.clang-format` is mandatory. Run `clang-format -i` on every modified
  `.c` / `.h` file before submitting.
- 8-space indent, Linux brace style, 80-column limit. Match the existing
  conventions; do not reformat unrelated code.
- The Meson build system is the single source of truth. Update
  `meson.build` / `tests/meson.build` when adding or removing source files.
- No CMake, no Make, no other build systems.

## C language rules

- C11 only (uses `_Static_assert`).
- Use fixed-width types from `<stdint.h>` and `<stdbool.h>`.
- No heap allocation (`malloc`, `free`, VLAs).
- Validate pointer arguments at every public-API boundary.
- Pointer-free internals: the doubly-linked list uses integer indices.

## Tests and coverage

- Add a test for every bug fix.
- Add a test for every new feature.
- CI enforces an 80% line / 70% branch coverage gate.
- Tests live in `tests/test_*.c`.

## Commits

Use Conventional Commits:

- `feat: ...` new feature
- `fix: ...` bug fix
- `doc: ...` documentation only
- `test: ...` test-only changes
- `chore: ...` build, CI, release work
- `refactor: ...` code change that neither fixes a bug nor adds a feature

Keep the subject under ~70 characters. Use the body to explain _why_ the
change is needed, not _what_ the diff already shows.

## Pull requests

- Open an issue first for non-trivial changes so the design can be agreed
  before implementation.
- Keep PRs focused. One feature or one fix per PR.
- All CI checks must pass: tests under ASan + UBSan, release
  build, coverage gate.

## When in doubt

Open an issue and discuss before writing code. The library is small
enough that even modest design changes have outsized implications.
