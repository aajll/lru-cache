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

# ── Version header ─────────────────────────────────────────────────────────────

ver = meson.project_version().split('.')
major = ver[0].to_int()
minor = ver[1].to_int()
patch = ver[2].to_int()

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set('MAJOR', major)
conf.set('MINOR', minor)
conf.set('PATCH', patch)

configure_file(
  input: 'config' / 'lru_cache_version.h.in',
  output: 'lru_cache_version.h',
  configuration: conf,
  install: true,
  install_dir: get_option('includedir') / 'lru_cache',
)

# ── Include directories ────────────────────────────────────────────────────────

public_headers = include_directories('include')

install_headers('include' / 'lru_cache.h', subdir: 'lru_cache')

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
