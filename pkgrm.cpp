//! \file  pkgrm.cpp
//! \brief pkgrm utility implementation.
//!        See COPYING and COPYRIGHT files for corresponding information.

#include <unistd.h>

#include "pkgrm.h"

void pkgrm::print_help() const
{
  cout << "Usage: " << utilname << R"( [OPTION] PKGNAME
Remove software package.

Mandatory arguments to long options are mandatory for short options too.
  -r, --root=PATH     specify alternative installation root
  -v, --verbose       explain what is being done
  -V, --version       print version and exit
  -h, --help          print help and exit
)";
}

void pkgrm::run(int argc, char** argv)
{
  /*
   * Check command line options.
   */
  static int o_verbose = 0;
  static string o_root, o_package;
  int opt;
  static struct option longopts[] = {
    { "root",     required_argument,  NULL,           'r' },
    { "verbose",  no_argument,        NULL,           'v' },
    { "version",  no_argument,        NULL,           'V' },
    { "help",     no_argument,        NULL,           'h' },
    { 0,          0,                  0,              0   },
  };

  while ((opt = getopt_long(argc, argv, ":r:vVh", longopts, 0)) != -1)
  {
    char ch = static_cast<char>(optopt);
    switch (opt) {
    case 'r':
      o_root = optarg;
      break;
    case 'v':
      o_verbose++;
      break;
    case 'V':
      return print_version();
      break;
    case 'h':
      return print_help();
      break;
    case ':':
      throw runtime_error("-"s + ch + ": missing option argument\n");
    case '?':
      throw runtime_error("-"s + ch + ": invalid option\n");
    }
  }

  if (optind == argc)
    throw runtime_error("missing package name");
  else if (argc - optind > 1)
    throw runtime_error("too many arguments");

  o_package = argv[optind];

  /*
   * Check UID.
   */
  if (getuid())
    throw runtime_error("only root can remove packages");

  /*
   * Remove package.
   */
  {
    db_lock lock(o_root, true);
    db_open(o_root);

    if (!db_find_pkg(o_package))
      throw runtime_error("package " + o_package + " not installed");

    if (o_verbose)
      cout << "removing " << o_package << endl;

    db_rm_pkg(o_package);
    ldconfig();
    db_commit();
  }
}

// vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
// End of file.
