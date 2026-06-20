conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())

configure_file(
  output: 'pkgutils-config.h',
  configuration: conf,
)

internal_inc = include_directories('.')
