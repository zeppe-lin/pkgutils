project(
  'pkgutils',
  'cpp',
  version: '6.4',
  meson_version: '>=1.2.0',
  default_options: [
    'cpp_std=c++11',
    'warning_level=3',
    'b_ndebug=if-release',
  ],
)

want_static = get_option('link_mode') == 'static'

libpkgcore_dep = dependency(
  'libpkgcore',
  required: true,
  static: want_static,
  fallback: ['libpkgcore', 'libpkgcore_dep'],
)

libpkgaudit_dep = dependency(
  'libpkgaudit',
  required: true,
  static: want_static,
  fallback: ['libpkgaudit', 'libpkgaudit_dep'],
)

subdir('internal')
subdir('src')
subdir('man')
subdir('completion')
subdir('vim')
subdir('tests')
