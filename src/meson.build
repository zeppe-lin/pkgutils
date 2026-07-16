common_deps = [libpkgcore_dep, libpkgaudit_dep]
common_incs = [internal_inc]
common_link_args = []

if want_static
  common_link_args += ['-static']
endif

exe_kwargs = {
  'dependencies': common_deps,
  'include_directories': common_incs,
  'link_args': common_link_args,
  'pie': not want_static,
  'install': true,
}

mutation_frontend_sources = ['deferred_signals.cpp']

executable('pkgadd', ['pkgadd.cpp'] + mutation_frontend_sources,
  kwargs: exe_kwargs, install_dir: get_option('sbindir'))

executable('pkgrm', ['pkgrm.cpp'] + mutation_frontend_sources,
  kwargs: exe_kwargs, install_dir: get_option('sbindir'))

executable('pkgchk',  'pkgchk.cpp',  kwargs: exe_kwargs, install_dir: get_option('bindir'))
executable('pkginfo', 'pkginfo.cpp', kwargs: exe_kwargs, install_dir: get_option('bindir'))
