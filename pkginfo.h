/* See COPYING and COPYRIGHT files for corresponding information. */

#ifndef PKGINFO_H
#define PKGINFO_H

#include <getopt.h>

#include "pkgutil.h"

class pkginfo : public pkgutil {
public:
  pkginfo() : pkgutil("pkginfo") {}

  virtual void run(int argc, char** argv);
  virtual void print_version() const;
  virtual void print_help() const;
};

#endif /* PKGINFO_H */

/* vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
 * End of file. */
