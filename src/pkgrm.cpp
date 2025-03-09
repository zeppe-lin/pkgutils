//! \file  pkgrm.cpp
//! \brief pkgrm utility implementation.
//!        See COPYING and COPYRIGHT files for corresponding information.

#include <iostream>
#include <string>
#include <stdexcept>
#include <unistd.h>
#include <getopt.h>

#include "libpkgutils.h"

using namespace std;

void
print_help()
{
  cout << R"(Usage: pkgrm [-Vhv] [-r rootdir] pkgname
Remove software package.

Mandatory arguments to long options are mandatory for short options too.
  -r, --root=rootdir  specify an alternate root directory
  -v, --verbose       explain what is being done
  -V, --version       print version and exit
  -h, --help          print help and exit
)";
}

void
print_version()
{
  // create a pkgutil object to access version info
  pkgutil util("pkgrm");
  util.print_version();
}

int main(int argc, char** argv)
{
  /*
   * Check command line options.
   */
  string o_root, o_package;
  int o_verbose = 0;
  int opt;

  static struct option longopts[] = {
    { "root",     required_argument,  NULL,  'r' },
    { "verbose",  no_argument,        NULL,  'v' },
    { "version",  no_argument,        NULL,  'V' },
    { "help",     no_argument,        NULL,  'h' },
    { 0,          0,                  0,     0   },
  };

  while ((opt = getopt_long(argc, argv, "r:vVh", longopts, 0)) != -1)
  {
    switch (opt) {
    case 'r':
      o_root = optarg;
      break;
    case 'v':
      o_verbose++;
      break;
    case 'V':
      print_version();
      return 0;
    case 'h':
      print_help();
      return 0;
    default:
      cerr << "Invalid option." << endl;
      print_help();
      return 1;
    }
  }

  if (optind == argc)
  {
    cerr << "error: missing package name" << endl;
    print_help();
    return 1;
  }
  else if (argc - optind > 1)
  {
    cerr << "error: too many arguments" << endl;
    print_help();
    return 1;
  }

  o_package = argv[optind];

  /*
   * Check UID.
   */
  if (getuid() != 0)
  {
    cerr << "error: only root can remove packages" << endl;
    return 1;
  }

  try
  {
    pkgutil util("pkgrm");
    db_lock lock(o_root, true);
    util.db_open(o_root);

    if (!util.db_find_pkg(o_package))
      throw runtime_error("package " + o_package + " not installed");

    if (o_verbose)
      cout << "removing " << o_package << endl;

    util.db_rm_pkg(o_package);
    util.ldconfig();
    util.db_commit();
  }
  catch (const runtime_error& error)
  {
    cerr << "error: " << error.what() << endl;
    return 1;
  }

  return EXIT_SUCCESS;
}

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.
