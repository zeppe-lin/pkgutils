common_deps = [pkgcore_dep]
common_includes = [config_inc]

common_link_args = []
if get_option('link_mode') == 'static'
  common_link_args += ['-static']
endif

exe_kwargs = {
  'dependencies': common_deps,
  'include_directories': common_includes,
  'link_args': common_link_args,
  'install': true,
}

executable('pkgadd',  'pkgadd.cpp',  kwargs: exe_kwargs, install_dir: get_option('sbindir'))
executable('pkgrm',   'pkgrm.cpp',   kwargs: exe_kwargs, install_dir: get_option('sbindir'))
executable('pkgchk',  'pkgchk.cpp',  kwargs: exe_kwargs, install_dir: get_option('bindir'))
executable('pkginfo', 'pkginfo.cpp', kwargs: exe_kwargs, install_dir: get_option('bindir'))
