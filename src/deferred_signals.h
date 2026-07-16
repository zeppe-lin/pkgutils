/*!
 * \file  deferred_signals.h
 * \brief Scoped termination deferral for mutating pkgutils frontends.
 *
 * \copyright See COPYING for license terms and COPYRIGHT for notices.
 */

#pragma once

#include <csignal>
#include <cstddef>

namespace pkgutils {

/*!
 * \class deferred_signals
 * \brief Records ordinary termination requests while a mutation
 *        sequence is allowed to reach its current consistency
 *        boundary.
 *
 * \details
 * The constructor temporarily replaces the process dispositions for
 * SIGHUP, SIGINT, SIGQUIT, and SIGTERM.  The first managed signal
 * delivered to the temporary handler while the object exists is
 * recorded instead of immediately terminating the process.
 * On destruction, the guard restores the previous dispositions and
 * writes the recorded signal number to the result object supplied by
 * the caller.
 *
 * pkgadd and pkgrm use this guard only around their mutating
 * operation sequences.  Argument parsing and read-only preflight
 * retain normal signal behavior.
 *
 * \warning Signal dispositions are process-wide.  This helper is
 *          intended only for the current single-threaded pkgutils
 *          frontends.  Guards must not be nested or used
 *          concurrently.
 *
 * \warning Deferring termination does not make package operations
 *          atomic.  It provides no rollback, crash recovery, or
 *          SIGKILL protection.
 */
class deferred_signals {
public:
  /*!
   * \brief Begin recording ordinary termination requests.
   * \param result Reference receiving zero or the first deferred
   *        signal.  The referenced object must outlive this guard.
   * \throws std::runtime_error If the process signal mask or managed
   *         signal disposition cannot be changed or restored.
   */
  explicit deferred_signals(int& result);

  /*!
   * \brief Restore the previous signal dispositions and publish the
   *        first deferred signal number.
   *
   * \details
   * The destructor never re-raises the signal.
   * The frontend converts the recorded number into its final exit
   * status after the mutation scope.
   */
  ~deferred_signals() noexcept;

  deferred_signals(const deferred_signals&) = delete;
  deferred_signals& operator=(const deferred_signals&) = delete;

private:
  //! Number of entries in \a termination_signals_ and \a previous_.
  enum { signal_count_ = 4 };

  /*!
   * \brief Async-signal-safe handler used while deferral is active.
   * \param signal Delivered signal number.
   *
   * \details
   * The handler only reads and writes volatile sig_atomic_t state.
   */
  static void handle(int signal) noexcept;

  /*!
   * \brief Restore all dispositions installed by this guard.
   * \return Zero if no managed signal was received, otherwise the
   *         first deferred signal number.
   */
  int restore() noexcept;

  //! Caller-owned destination for the deferred signal number.
  int& result_;

  //! Managed signals, indexed in parallel with \a previous_.
  static const int termination_signals_[signal_count_];

  //! Original dispositions, indexed in parallel with
  //! \a termination_signals_.
  struct sigaction previous_[signal_count_];

  //! Number of dispositions successfully installed by the
  //! constructor.
  std::size_t installed_;

  //! Whether destruction still needs to restore process dispositions.
  bool active_;
};

} // namespace pkgutils
