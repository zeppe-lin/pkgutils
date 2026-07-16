if get_option('tests').allowed()
  # Exercise the frontend helper without linking libpkgcore:
  # the test verifies process signal policy only.
  deferred_signals_test = executable(
    'deferred-signals-test',
    [
      'deferred_signals_test.cpp',
      '../src/deferred_signals.cpp',
    ],
    include_directories: include_directories('../src'),
  )

  test('deferred frontend signals', deferred_signals_test)
endif
