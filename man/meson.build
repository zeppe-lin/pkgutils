if get_option('build_man')
  scdoc = find_program('scdoc')

  foreach section, pages: {
    '1': ['pkgchk', 'pkginfo'],
    '5': ['pkgadd.conf'],
    '7': ['packages'],
    '8': ['pkgadd', 'pkgrm'],
  }
    foreach page: pages
      custom_target(
        page + '.' + section,
        input: page + '.' + section + '.scdoc',
        output: page + '.' + section,
        command: [scdoc],
        capture: true,
        install: true,
        install_dir: get_option('mandir') / ('man' + section),
        install_tag: 'man',
      )
    endforeach
  endforeach
endif
