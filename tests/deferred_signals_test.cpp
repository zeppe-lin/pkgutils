/*!
 * \file  deferred_signals_test.cpp
 * \brief Regression test for scoped frontend signal deferral.
 */

#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <iostream>

#include "deferred_signals.h"

namespace {

//! Signal observed by the caller's original disposition.
volatile sig_atomic_t observed_signal = 0;

const int test_signals[] = {
  SIGHUP,
  SIGINT,
  SIGQUIT,
  SIGTERM,
};

constexpr std::size_t test_signal_count =
  sizeof(test_signals) / sizeof(test_signals[0]);

void
record_signal(int signal)
{
  observed_signal = signal;
}

bool
managed_masks_equal(const sigset_t& lhs, const sigset_t& rhs)
{
  for (std::size_t i = 0; i < test_signal_count; ++i)
  {
    const int left = sigismember(&lhs, test_signals[i]);
    const int right = sigismember(&rhs, test_signals[i]);

    if (left == -1 || right == -1 || left != right)
      return false;
  }

  return true;
}

/*!
 * \brief Exercise signal recording and disposition restoration.
 * \return True if every tested invariant holds.
 */
bool
run_test()
{
  sigset_t mask_before;
  sigset_t mask_after;

  if (sigprocmask(SIG_SETMASK, 0, &mask_before) == -1)
  {
    std::cerr << "could not inspect signal mask before guard"
              << std::endl;
    return false;
  }

  int deferred_signal = 0;
  {
    pkgutils::deferred_signals signals(deferred_signal);

    /*
     * The temporary handler must consume the request without invoking
     * the disposition that was active before the guard.
     */
    if (raise(SIGTERM) != 0)
    {
      std::cerr << "could not raise SIGTERM" << std::endl;
      return false;
    }

    if (observed_signal != 0)
    {
      std::cerr << "SIGTERM was not deferred" << std::endl;
      return false;
    }

    /*
     * The first request is authoritative.  A later managed signal
     * must not replace it.
     */
    if (raise(SIGINT) != 0)
    {
      std::cerr << "could not raise SIGINT" << std::endl;
      return false;
    }

    if (observed_signal != 0)
    {
      std::cerr << "SIGINT was not deferred" << std::endl;
      return false;
    }
  }

  if (sigprocmask(SIG_SETMASK, 0, &mask_after) == -1)
  {
    std::cerr << "could not inspect signal mask after guard"
              << std::endl;
    return false;
  }

  if (!managed_masks_equal(mask_before, mask_after))
  {
    std::cerr << "managed signal mask was not restored" << std::endl;
    return false;
  }

  /*
   * The guard publishes the deferred signal from its destructor.
   */
  if (deferred_signal != SIGTERM)
  {
    std::cerr << "first deferred signal was not reported"
              << std::endl;
    return false;
  }

  /*
   * Verify that leaving the guard restored the known disposition for
   * every managed signal.
   */
  for (std::size_t i = 0; i < test_signal_count; ++i)
  {
    struct sigaction current;

    if (sigaction(test_signals[i], 0, &current) == -1)
    {
      std::cerr << "could not inspect restored disposition for signal "
                << test_signals[i] << std::endl;
      return false;
    }

    if (current.sa_handler != record_signal)
    {
      std::cerr << "previous disposition was not restored for signal "
                << test_signals[i] << std::endl;
      return false;
    }
  }

  /*
   * Prove that delivery after the guard once again follows the
   * dispositions installed by the caller.
   */
  for (std::size_t i = 0; i < test_signal_count; ++i)
  {
    observed_signal = 0;

    if (raise(test_signals[i]) != 0)
    {
      std::cerr << "could not raise signal "
                << test_signals[i] << std::endl;
      return false;
    }

    if (observed_signal != test_signals[i])
    {
      std::cerr << "restored handler did not receive signal "
                << test_signals[i] << std::endl;
      return false;
    }
  }

  return true;
}

} // namespace

int
main()
{
  struct sigaction action;
  struct sigaction previous[test_signal_count];
  std::size_t installed = 0;

  action.sa_handler = record_signal;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  /*
   * Install one known disposition for every managed signal.
   * The guard must preserve and later restore each of them.
   */
  for (std::size_t i = 0; i < test_signal_count; ++i)
  {
    if (sigaction(test_signals[i], &action, &previous[i]) == -1)
    {
      std::cerr << "could not install test disposition for signal "
                << test_signals[i] << std::endl;

      break;
    }

    ++installed;
  }

  bool success = false;

  if (installed == test_signal_count)
    success = run_test();

  /*
   * Restore the process state that existed before the test, including
   * the partial-installation case.
   */
  while (installed > 0)
  {
    --installed;

    if (sigaction(test_signals[installed], &previous[installed], 0) == -1)
    {
      std::cerr << "could not restore original disposition for signal "
                << test_signals[installed] << std::endl;
      success = false;
    }
  }

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
