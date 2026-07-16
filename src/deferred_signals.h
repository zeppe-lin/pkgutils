#pragma once

#include <csignal>
#include <cstddef>

namespace pkgutils {

/*
 * Temporarily defer ordinary termination requests.
 *
 * The first SIGHUP, SIGINT, SIGQUIT, or SIGTERM received while this
 * object exists is recorded.  Previous dispositions are restored when
 * the object is destroyed, and the recorded signal is written to the
 * caller-provided result.
 *
 * This helper is process policy for the single-threaded pkgutils
 * frontends.  It is not a transaction or rollback mechanism.
 */
class deferred_signals {
public:
  explicit deferred_signals(int& result);
  ~deferred_signals();

  deferred_signals(const deferred_signals&) = delete;
  deferred_signals& operator=(const deferred_signals&) = delete;

private:
  static void handle(int signal);
  int restore();

  int& result;
  struct sigaction previous[4];
  std::size_t installed;
  bool active;
};

} // namespace pkgutils
