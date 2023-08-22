//! \file  main.cpp
//! \brief Command-line utility of \a pkgutils.
//!        See COPYING and COPYRIGHT files for corresponding information.

#if (__GNUC__ < 3)
# error This program requires GCC 3.x to compile.
#endif

#include <iostream>
#include <string>
#include <memory>
#include <cstdlib>

#include <libgen.h>

#include "pkgutil.h"
#include "pkgadd.h"
#include "pkgrm.h"
#include "pkginfo.h"

using namespace std;

static pkgutil* select_utility(const string& name)
{
  if (name == "pkgadd")
    return new pkgadd;
  else if (name == "pkgrm")
    return new pkgrm;
  else if (name == "pkginfo")
    return new pkginfo;
  else
    throw runtime_error("command not supported by pkgutils");
}

int main(int argc, char** argv)
{
  try
  {
    string name = basename(argv[0]);
    unique_ptr<pkgutil> util(select_utility(name));
    util->run(argc, argv);
  }
  catch (invalid_argument& e)
  {
    if (strlen(e.what()))
      cerr << argv[0] << ": " << e.what() << endl;
    cerr << "Try '" << argv[0] << " --help' for more information." << endl;
    return EXIT_FAILURE;
  }
  catch (runtime_error& e)
  {
    cerr << argv[0] << ": " << e.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

// vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
// End of file.
