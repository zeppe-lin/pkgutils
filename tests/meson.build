deferred_signals_test = executable(
  'deferred-signals-test',
  [
    'deferred_signals_test.cpp',
    '../src/deferred_signals.cpp',
  ],
  include_directories: include_directories('../src'),
)

test('deferred frontend signals', deferred_signals_test)
