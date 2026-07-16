/*!
 * \file  deferred_signals.cpp
 * \brief Implementation of scoped frontend termination deferral.
 *
 * \copyright See COPYING for license terms and COPYRIGHT for notices.
 */
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#include "deferred_signals.h"

namespace {

/*!
 * \brief First termination request observed by the temporary handler.
 *
 * \details
 * This is the only shared state touched from signal context.
 * A value of zero means that no managed signal has been received.
 */
volatile sig_atomic_t pending_signal = 0;

} // namespace

namespace pkgutils {

//! Signals deferred by the mutating pkgutils frontends.
const int
deferred_signals::termination_signals_[
    deferred_signals::signal_count_] = {
  SIGHUP,
  SIGINT,
  SIGQUIT,
  SIGTERM,
};


void
deferred_signals::handle(int signal) noexcept
{
  /*
   * Signal handlers may call only async-signal-safe operations.
   * Keep the first request and perform all reporting, cleanup, and
   * exit-status work later in ordinary control flow.
   */
  if (pending_signal == 0)
    pending_signal = signal;
}

deferred_signals::deferred_signals(int& result)
    : result_(result), installed_(0), active_(false)
{
  result_ = 0;

  struct sigaction action;
  sigset_t managed_mask;
  sigset_t previous_mask;

  std::memset(&action, 0, sizeof(action));
  action.sa_handler = handle;

  sigemptyset(&action.sa_mask);
  sigemptyset(&managed_mask);

  /*
   * The handler mask prevents managed signals from nesting inside
   * handle().  The process mask protects installation and restoration
   * of the process-wide dispositions.
   */
  for (std::size_t i = 0; i < signal_count_; ++i)
  {
    sigaddset(&action.sa_mask, termination_signals_[i]);
    sigaddset(&managed_mask, termination_signals_[i]);
  }

  /*
   * Block managed signals while replacing their process-wide
   * dispositions.  This prevents delivery through a partially
   * installed signal policy.
   */
  if (sigprocmask(SIG_BLOCK, &managed_mask, &previous_mask) == -1)
  {
    throw std::runtime_error(
        std::string("could not block termination signals: ") +
        std::strerror(errno));
  }

  pending_signal = 0;

  /*
   * Restart interrupted system calls where supported.  This reduces
   * incidental EINTR failures; it is not a transaction guarantee.
   */
  action.sa_flags = SA_RESTART;

  for (std::size_t i = 0; i < signal_count_; ++i)
  {
    if (sigaction(termination_signals_[i], &action,
                  &previous_[i]) == -1)
    {
      const int saved_errno = errno;

      /*
       * Restore dispositions already replaced by this instance while
       * managed signals remain blocked.
       */
      while (installed_ > 0)
      {
        --installed_;
        sigaction(termination_signals_[installed_],
                  &previous_[installed_], 0);
      }

      /*
       * A failed constructor has no destructor to restore process
       * state.  Report mask-restoration failure together with the
       * original error.
       */
      if (sigprocmask(SIG_SETMASK, &previous_mask, 0) == -1)
      {
        const int mask_errno = errno;

        throw std::runtime_error(
            std::string("could not defer termination signals: ") +
            std::strerror(saved_errno) +
            "; could not restore signal mask: " +
            std::strerror(mask_errno));
      }

      throw std::runtime_error(
          std::string("could not defer termination signals: ") +
          std::strerror(saved_errno));
    }

    ++installed_;
  }

  /*
   * Mark the guard active before unblocking.  A signal that becomes
   * pending during installation may be delivered immediately when the
   * caller's mask is restored.
   */
  active_ = true;

  if (sigprocmask(SIG_SETMASK, &previous_mask, 0) == -1)
  {
    const int saved_errno = errno;

    /*
     * Roll back the installed policy while managed signals are still
     * expected to be blocked.
     */
    while (installed_ > 0)
    {
      --installed_;
      sigaction(termination_signals_[installed_],
                &previous_[installed_], 0);
    }

    active_ = false;
    pending_signal = 0;

    /*
     * Make one best-effort attempt to restore the caller's mask after
     * the failed restoration.
     */
    sigprocmask(SIG_SETMASK, &previous_mask, 0);

    throw std::runtime_error(
        std::string("could not restore signal mask: ") +
        std::strerror(saved_errno));
  }
}

deferred_signals::~deferred_signals() noexcept
{
  if (active_)
    result_ = restore();
}

int
deferred_signals::restore() noexcept
{
  sigset_t managed_mask;
  sigset_t previous_mask;

  sigemptyset(&managed_mask);

  for (std::size_t i = 0; i < signal_count_; ++i)
    sigaddset(&managed_mask, termination_signals_[i]);

  /*
   * Attempt to block managed signals while restoring the caller's
   * dispositions.  This makes the transition indivisible with respect
   * to managed signal delivery when masking succeeds.
   *
   * Restoration remains best-effort because this function is called
   * from a noexcept destructor.
   */
  const bool masked =
    sigprocmask(SIG_BLOCK, &managed_mask, &previous_mask) == 0;

  /*
   * Preserve the first deferred signal before clearing handler state.
   */
  const int deferred = pending_signal;

  /*
   * Restore in reverse installation order.  A noexcept destructor can
   * only perform best-effort restoration.
   */
  while (installed_ > 0)
  {
    --installed_;
    sigaction(termination_signals_[installed_],
              &previous_[installed_], 0);
  }

  active_ = false;
  pending_signal = 0;

  if (masked)
    sigprocmask(SIG_SETMASK, &previous_mask, 0);

  return deferred;
}

} // namespace pkgutils
