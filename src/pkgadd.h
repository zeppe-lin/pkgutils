//! \file  pkgadd.h
//! \brief pkgadd class definition.
//!        See COPYING and COPYRIGHT files for corresponding information.

#pragma once

#include <vector>
#include <set>

#include <getopt.h>

#include "pkgutil.h"
#include "pathnames.h"

enum rule_event_t {
  UPGRADE,
  INSTALL
};

struct rule_t {
  rule_event_t  event;
  string        pattern;
  bool          action;
};

class pkgadd : public pkgutil
{
public:
  pkgadd() : pkgutil("pkgadd") {}

  virtual void run(int argc, char** argv) override;
  virtual void print_help() const override;

private:
  vector<rule_t> read_config(const string& file) const;

  set<string> make_keep_list(const set<string>&     files,
                             const vector<rule_t>&  rules) const;

  set<string> apply_install_rules(const string&          name,
                                  pkginfo_t&             info,
                                  const vector<rule_t>&  rules);

  void find_rules(const vector<rule_t>&  rules,
                  rule_event_t           event,
                  vector<rule_t>&        found) const;

  bool rule_applies_to_file(const rule_t&  rule,
                            const string&  file) const;
}; // class pkgadd

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.
