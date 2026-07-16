#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#include "deferred_signals.h"

namespace {

volatile sig_atomic_t pending_signal = 0;

const int termination_signals[] = {
  SIGHUP,
  SIGINT,
  SIGQUIT,
  SIGTERM,
};

} // namespace

namespace pkgutils {

void
deferred_signals::handle(int signal)
{
  if (pending_signal == 0)
    pending_signal = signal;
}

deferred_signals::deferred_signals(int& result_)
    : result(result_), installed(0), active(false)
{
  result = 0;
  pending_signal = 0;

  struct sigaction action;
  std::memset(&action, 0, sizeof(action));
  action.sa_handler = handle;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_RESTART;

  for (std::size_t i = 0; i < 4; ++i)
  {
    if (sigaction(termination_signals[i], &action,
                  &previous[i]) == -1)
    {
      const int saved_errno = errno;

      while (installed > 0)
      {
        --installed;
        sigaction(termination_signals[installed],
                  &previous[installed], 0);
      }

      throw std::runtime_error(
          std::string("could not defer termination signals: ") +
          std::strerror(saved_errno));
    }

    ++installed;
  }

  active = true;
}

deferred_signals::~deferred_signals()
{
  if (active)
    result = restore();
}

int
deferred_signals::restore()
{
  const int deferred = pending_signal;

  while (installed > 0)
  {
    --installed;
    sigaction(termination_signals[installed],
              &previous[installed], 0);
  }

  active = false;
  pending_signal = 0;
  return deferred;
}

} // namespace pkgutils
