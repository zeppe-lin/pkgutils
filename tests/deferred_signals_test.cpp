#include "deferred_signals.h"

#include <csignal>
#include <cstdlib>
#include <iostream>

namespace {

volatile sig_atomic_t observed_signal = 0;

void
record_signal(int signal)
{
  observed_signal = signal;
}

} // namespace

int
main()
{
  struct sigaction action;
  struct sigaction previous;
  struct sigaction current;

  action.sa_handler = record_signal;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  if (sigaction(SIGTERM, &action, &previous) == -1)
  {
    std::cerr << "could not install test signal handler" << std::endl;
    return EXIT_FAILURE;
  }

  int deferred_signal = 0;
  {
    pkgutils::deferred_signals signals(deferred_signal);

    if (raise(SIGTERM) != 0)
      return EXIT_FAILURE;

    if (observed_signal != 0)
    {
      std::cerr << "signal was not deferred" << std::endl;
      return EXIT_FAILURE;
    }
  }

  if (deferred_signal != SIGTERM)
  {
    std::cerr << "deferred signal was not reported" << std::endl;
    return EXIT_FAILURE;
  }

  if (sigaction(SIGTERM, 0, &current) == -1 ||
      current.sa_handler != record_signal)
  {
    std::cerr << "previous signal disposition was not restored"
              << std::endl;
    return EXIT_FAILURE;
  }

  if (raise(SIGTERM) != 0 || observed_signal != SIGTERM)
    return EXIT_FAILURE;

  sigaction(SIGTERM, &previous, 0);
  return EXIT_SUCCESS;
}
