//! \file  pkgrm.h
//! \brief pkgrm class definition.
//!        See COPYING and COPYRIGHT files for corresponding information.

#pragma once

#include <getopt.h>

#include "pkgutil.h"

class pkgrm : public pkgutil
{
public:
  pkgrm() : pkgutil("pkgrm") {}

  virtual void run(int argc, char** argv) override;
  virtual void print_help() const override;
}; // class pkgrm

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.
