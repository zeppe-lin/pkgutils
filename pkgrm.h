/* See COPYING and COPYRIGHT files for corresponding information. */

#ifndef PKGRM_H
#define PKGRM_H

#include <getopt.h>
#include "pkgutil.h"

class pkgrm : public pkgutil {
public:
  pkgrm() : pkgutil("pkgrm") {}

  virtual void run(int argc, char** argv);
  virtual void print_version() const;
  virtual void print_help() const;
};

#endif /* PKGRM_H */

/* vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
 * End of file. */
