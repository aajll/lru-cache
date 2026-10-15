# SPDX-License-Identifier: MIT

project(
  'lru-cache',
  'c',
  version: '1.1.7',
  license: 'MIT',
  default_options: [
    'c_std=c11',
    'warning_level=3',
  ],
  meson_version: '>=0.57.0',
)

project_description = 'A static-memory, deterministic Least Recently Used (LRU) cache.'

# ── Version ────────────────────────────────────────────────────────────────────
# include/lru_cache.h defines the version macros, so a copy-in build has them.
# Configuration fails when they differ from the project version.

fs = import('fs')
version_parts = meson.project_version().split('.')
version_macros = {
  'LRU_CACHE_VERSION_MAJOR': version_parts[0],
  'LRU_CACHE_VERSION_MINOR': version_parts[1],
  'LRU_CACHE_VERSION_PATCH': version_parts[2],
  'LRU_CACHE_VERSION': '"' + meson.project_version() + '"',
}
header_lines = []
foreach line : fs.read('include' / 'lru_cache.h').split('\n')
  header_lines += ' '.join(line.split())
endforeach
foreach name, value : version_macros
  if '#define @0@ @1@'.format(name, value) not in header_lines
    error('include/lru_cache.h must define @0@ as @1@'.format(name, value))
  endif
endforeach

# ── Include directories ────────────────────────────────────────────────────────

public_headers = include_directories('include')

install_headers(
  'include' / 'lru_cache.h',
  'include' / 'lru_cache_version.h',
  subdir: 'lru_cache',
)

# ── Application core library ───────────────────────────────────────────────────

library_src = files('src/lru_cache.c')

if get_option('lookup_strategy') == 'linear'
  lru_strategy_arg = '-DLRU_CACHE_LOOKUP_STRATEGY=LRU_CACHE_LOOKUP_LINEAR'
else
  lru_strategy_arg = '-DLRU_CACHE_LOOKUP_STRATEGY=LRU_CACHE_LOOKUP_HASH'
endif

lru_cache_lib = static_library(
  'lru-cache',
  library_src,
  c_args: lru_strategy_arg,
  include_directories: public_headers,
  install: true,
)

lru_cache_dep = declare_dependency(
  compile_args: lru_strategy_arg,
  include_directories: public_headers,
  link_with: lru_cache_lib,
)

meson.override_dependency('lru-cache', lru_cache_dep)

# ── pkg-config ─────────────────────────────────────────────────────────────────

pkg = import('pkgconfig')
pkg.generate(
  name: 'lru-cache',
  description: project_description,
  filebase: 'lru-cache',
  version: meson.project_version(),
)

# ── Host unit tests ────────────────────────────────────────────────────────────

if get_option('build_tests')
  subdir('tests')
endif
