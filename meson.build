project(
  'pkgutils',
  'cpp',
  version: '6.3',
  meson_version: '>=1.2.0',
  default_options: [
    'cpp_std=c++11',
    'warning_level=3',
    'b_ndebug=if-release',
    'b_lto=false',
  ],
)

conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())

configure_file(
  output: 'pkgutils-config.h',
  configuration: conf,
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

config_inc = include_directories('.')

subdir('src')
subdir('man')
subdir('completion')
subdir('vim')
