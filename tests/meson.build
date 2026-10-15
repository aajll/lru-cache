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
