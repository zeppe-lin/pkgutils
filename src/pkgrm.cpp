//! \file  pkgrm.cpp
//! \brief pkgrm utility implementation.
//!
//! The `pkgrm` utility is used to remove software packages
//! from the system. It handles package removal and related cleanup
//! tasks.
//!
//! \copyright See COPYING and COPYRIGHT files for corresponding
//!            information.

#include <iostream>
#include <string>
#include <stdexcept>
#include <unistd.h>
#include <getopt.h>

#include "libpkgutils.h"

using namespace std;

// Forward declarations
void print_help();
void print_version();

/*!
 * \brief Prints the help message for pkgrm utility.
 *
 * Displays usage instructions, available options, and a brief
 * description of the `pkgrm` utility's functionality to standard
 * output.
 */
void
print_help()
{
  cout << R"(Usage: pkgrm [-Vhv] [-r rootdir] pkgname
Remove software package.

Mandatory arguments to long options are mandatory for short options too.
  -r, --root=rootdir    specify an alternate root directory
  -v, --verbose       explain what is being done
  -V, --version       print version and exit
  -h, --help          print help and exit
)";
}

/*!
 * \brief Prints the version information for pkgrm utility.
 *
 * Retrieves the version string from the pkgutil library and
 * displays it to the user via standard output.
 */
void
print_version()
{
  pkgutil util("pkgrm");
  util.print_version();
}

/*!
 * \brief Main function for the pkgrm utility.
 * \param argc Argument count from command line.
 * \param argv Argument vector from command line.
 * \return EXIT_SUCCESS on successful execution, EXIT_FAILURE on error.
 *
 * This function parses command line arguments, performs package
 * removal, and handles error conditions.
*/
int
main(int argc, char** argv)
{
  // --- Option Parsing ---
  string o_root, o_package;
  int o_verbose = 0;
  int opt;

  static struct option longopts[] = {
    { "root",    required_argument, NULL, 'r' },
    { "verbose", no_argument,       NULL, 'v' },
    { "version", no_argument,       NULL, 'V' },
    { "help",    no_argument,       NULL, 'h' },
    { 0,         0,                 0,    0   },
  };

  while ((opt = getopt_long(argc, argv, "r:vVh", longopts, 0)) != -1)
  {
    switch (opt)
    {
      case 'r': o_root = optarg;  break;
      case 'v': o_verbose++;      break;
      case 'V': print_version();  return EXIT_SUCCESS;
      case 'h': print_help();     return EXIT_SUCCESS;
      default:
        cerr << "Invalid option." << endl;
        print_help();
        return EXIT_FAILURE;
    }
  }

  // --- Argument Validation ---
  if (optind == argc)
  {
    cerr << "error: missing package name" << endl;
    print_help();
    return EXIT_FAILURE;
  }
  else if (argc - optind > 1)
  {
    cerr << "error: too many arguments" << endl;
    print_help();
    return EXIT_FAILURE;
  }

  o_package = argv[optind];

  // --- Privilege Check ---
  if (getuid() != 0)
  {
    cerr << "error: only root can remove packages" << endl;
    return EXIT_FAILURE;
  }

  // --- Package Removal Logic ---
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
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.
