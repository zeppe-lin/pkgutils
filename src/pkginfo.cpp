/*!
 * \file  pkginfo.cpp
 * \brief pkginfo utility implementation.
 *
 * The `pkginfo` utility is used to display information about software
 * packages and package files.  It can list installed packages,
 * display package contents, and show file ownership.
 *
 * \copyright See COPYING for license terms and COPYRIGHT for notices.
 */

#include <algorithm>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <iomanip>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <regex.h>
#include <iterator> // Required for ostream_iterator

#include "libpkgutils.h" // Include the library header

using namespace std;

// Forward declarations
void print_help();
void print_version();

/*!
 * \brief Prints the help message for pkginfo utility.
 *
 * Displays usage instructions, available options, and a brief
 * description of the pkginfo utility's functionality to standard
 * output.
 */
void
print_help()
{
  cout << R"(Usage: pkginfo [-Vh] [-r root-dir]
               {-f package-archive | -l package | -o path-pattern | -i}
Display software package information.

Mandatory arguments to long options are mandatory for short options too.
  -r, --root=root-dir       Use an alternate root directory
  -f, --footprint=package-archive
                            Print a package's footprint
  -l, --list=package        List files owned by an installed package
                            or contained in package archive
  -o, --owner=path-pattern  List packages that own files matching a
                            pattern
  -i, --installed           List installed packages and their versions
  -V, --version             Print version and exit
  -h, --help                Print this help and exit
)";
}

/*!
 * \brief Prints the version information for pkginfo utility.
 *
 * Retrieves the version string from the pkgutil library and displays
 * it to the user via standard output.
 */
void
print_version()
{
    pkgutil util("pkginfo");
    util.print_version();
}

/*!
 * \brief Main function for the pkginfo utility.
 * \param argc Argument count from command line.
 * \param argv Argument vector from command line.
 * \return EXIT_SUCCESS on successful execution,
 *         EXIT_FAILURE on error.
 *
 * Parses command line arguments, retrieves and displays package
 * information based on the selected mode (footprint, installed
 * packages, list package contents, owner lookup), and handles error
 * conditions.
 */
int
main(int argc, char** argv)
{
  // --- Option Parsing ---
  int o_footprint_mode = 0;
  int o_installed_mode = 0;
  int o_list_mode     = 0;
  int o_owner_mode    = 0;
  string o_root;
  string o_arg;
  int opt;
  static struct option longopts[] = {
    { "footprint",  required_argument, NULL, 'f' },
    { "installed",  no_argument,       NULL, 'i' },
    { "list",       required_argument, NULL, 'l' },
    { "owner",      required_argument, NULL, 'o' },
    { "root",       required_argument, NULL, 'r' },
    { "version",    no_argument,       NULL, 'V' },
    { "help",       no_argument,       NULL, 'h' },
  };

  while ((opt = getopt_long(argc, argv, "f:il:o:r:Vh", longopts, 0)) != -1)
  {
    switch (opt)
    {
      case 'f':
        o_footprint_mode = 1;
        o_arg = optarg;
        break;
      case 'i':
        o_installed_mode = 1;
        break;
      case 'l':
        o_list_mode = 1;
        o_arg = optarg;
        break;
      case 'o':
        o_owner_mode = 1;
        o_arg = optarg;
        break;
      case 'r':
        o_root = optarg;
        break;
      case 'V':
        print_version();
        return EXIT_SUCCESS;
      case 'h':
        print_help();
        return EXIT_SUCCESS;
      default:
        cerr << "Invalid option." << endl;
        print_help();
        return EXIT_FAILURE;
    }
  }

  // --- Argument Validation ---
  if (o_footprint_mode + o_installed_mode + o_list_mode +
      o_owner_mode == 0)
  {
    cerr << "error: option missing" << endl;
    print_help();
    return EXIT_FAILURE;
  }

  if (o_footprint_mode + o_installed_mode + o_list_mode +
      o_owner_mode > 1)
  {
    cerr << "error: too many options" << endl;
    print_help();
    return EXIT_FAILURE;
  }

  pkgutil util("pkginfo");

  // --- Mode Execution ---
  if (o_footprint_mode)
  {
    // --- Footprint Mode ---
    try
    {
      util.pkg_footprint(o_arg);
    }
    catch (const runtime_error& error)
    {
      cerr << "error: " << error.what() << endl;
      return EXIT_FAILURE;
    }
  }
  else
  {
    // --- Modes Requiring Database Access ---
    try
    {
      db_lock lock(o_root, false);
      util.db_open(o_root);
    }
    catch (const runtime_error& error)
    {
      cerr << "error: " << error.what() << endl;
      return EXIT_FAILURE;
    }

    if (o_installed_mode)
    {
      // --- Installed Packages Mode ---
      for (const auto& package_pair : util.getPackages())
        cout << package_pair.first << ' '
             << package_pair.second.version << endl;
    }
    else if (o_list_mode)
    {
      // --- List Package/File Contents Mode ---
      try
      {
        if (util.db_find_pkg(o_arg))
        {
          copy(util.getPackages().at(o_arg).files.begin(),
               util.getPackages().at(o_arg).files.end(),
               ostream_iterator<string>(cout, "\n"));
        }
        else if (file_exists(o_arg))
        {
          pair<string, pkgutil::pkginfo_t> package =
                                          util.pkg_open(o_arg);
          copy(package.second.files.begin(),
               package.second.files.end(),
               ostream_iterator<string>(cout, "\n"));
        }
        else
        {
          throw runtime_error(o_arg +
              " is neither an installed package nor a package file");
        }
      }
      catch (const runtime_error& error)
      {
        cerr << "error: " << error.what() << endl;
        return EXIT_FAILURE;
      }
    }
    else
    {
      // --- Owner Lookup Mode ---
      regex_t preg;
      if (regcomp(&preg, o_arg.c_str(),
                  REG_EXTENDED | REG_NOSUB))
      {
        cerr << "error: fail to compile regular expression '"
             << o_arg << "', aborting" << endl;
        return EXIT_FAILURE;
      }

      vector<pair<string, string> > result;
      result.push_back({"Package", "File"});

      // "Package" width
      int width = static_cast<int>(result[0].first.length());

      for (const auto& package_pair : util.getPackages())
      {
        for (const string& file : package_pair.second.files)
        {
          const string full_file = '/' + file;

          if (!regexec(&preg, full_file.c_str(), 0, 0, 0))
          {
            result.push_back({package_pair.first, file});
            width = max(width, static_cast<int>(package_pair.first.length()));
          }
        }
      }

      regfree(&preg);

      if (result.size() > 1)
      {
        for (const auto& res_pair : result)
          cout << left << setw(width + 2)
               << res_pair.first << res_pair.second << endl;
      }
      else
        cout << "pkginfo: no owner(s) found" << endl;
    }
  }

  return EXIT_SUCCESS;
}
