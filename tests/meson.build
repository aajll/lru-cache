# SPDX-License-Identifier: MIT

# ---------------------------------------------------------------------------
# Test suite
#
# The same test source runs once per lookup strategy. The library and the
# test must agree on the strategy, so each gets its own build of the
# library.
# ---------------------------------------------------------------------------

foreach strategy : ['hash', 'linear']
  strategy_arg = '-DLRU_CACHE_LOOKUP_STRATEGY=LRU_CACHE_LOOKUP_' + strategy.to_upper()

  strategy_lib = static_library(
    'lru-cache-' + strategy,
    library_src,
    c_args: strategy_arg,
    include_directories: public_headers,
  )

  test_exe = executable(
    'test_lru_' + strategy,
    ['test_lru_cache.c'],
    c_args: strategy_arg,
    link_with: strategy_lib,
    include_directories: [public_headers],
  )

  test(
    'lru_cache / ' + strategy + ' strategy',
    test_exe,
    suite: 'unit',
    timeout: 30,
  )
endforeach

# Every C code block in the README, CONTRIBUTING.md and docs/ compiles against
# the public header, so a documented signature cannot drift from the code.
python = import('python').find_installation('python3')

test(
  'lru_cache docs examples',
  python,
  args: [
    files('docs/check_snippets.py'),
    '--root', meson.project_source_root(),
    '--lang', 'c',
    '--prelude', '#include "lru_cache.h"',
    '--',
  ] + meson.get_compiler('c').cmd_array() + [
    '-std=c11',
    '-Wall',
    '-Wextra',
    '-Wpedantic',
    '-Werror',
    '-Wno-unused-function',
    '-Wno-unused-variable',
    '-I' + (meson.project_source_root() / 'include'),
    '-c', '{source}', '-o', '{object}',
  ],
  suite: 'docs',
  timeout: 60,
)

# The hash table is indexed by uint16_t, so a table larger than UINT16_MAX
# must fail the build, and the largest valid size must still compile.
cc = meson.get_compiler('c')
table_size_src = '#define LRU_CACHE_HASH_TABLE_SIZE @0@U\n#include "lru_cache.h"\n'
assert(
  not cc.compiles(table_size_src.format(65536), include_directories: public_headers),
  'LRU_CACHE_HASH_TABLE_SIZE above 65535 must fail the build',
)
assert(
  cc.compiles(table_size_src.format(65535), include_directories: public_headers),
  'LRU_CACHE_HASH_TABLE_SIZE of 65535 must compile',
)
