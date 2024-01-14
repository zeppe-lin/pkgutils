//! \file  pkginfo.h
//! \brief pkginfo class definition.
//!        See COPYING and COPYRIGHT files for corresponding information.

#pragma once

#include <getopt.h>

#include "pkgutil.h"

class pkginfo : public pkgutil
{
public:
  pkginfo() : pkgutil("pkginfo") {}

  virtual void run(int argc, char** argv) override;
  virtual void print_help() const override;
}; // class pkginfo

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.
