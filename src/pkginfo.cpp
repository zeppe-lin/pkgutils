//! \file  pkginfo.cpp
//! \brief pkginfo utility implementation.
//!        See COPYING and COPYRIGHT files for corresponding information.

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

#include "libpkgutils.h"

using namespace std;

void
print_help()
{
  cout << R"(Usage: pkginfo [-Vh] [-r rootdir]
               {-f file | -i | -l <pkgname | file> | -o pattern}
Display software package information.

Mandatory arguments to long options are mandatory for short options too.
  -f, --footprint=file         print footprint for file
  -i, --installed              list installed packages and their version
  -l, --list=<pkgname | file>  list files in package or file
  -o, --owner=pattern          list owner(s) of file(s) matching pattern
  -r, --root=rootdir           specify an alternate root directory
  -V, --version                print version and exit
  -h, --help                   print help and exit
)";
}

void
print_version()
{
  pkgutil util("pkginfo");
  util.print_version();
}

int
main(int argc, char** argv)
{
  /*
   * Check command line options.
   */
  int o_footprint_mode = 0;
  int o_installed_mode = 0;
  int o_list_mode      = 0;
  int o_owner_mode     = 0;
  string o_root;
  string o_arg;
  int opt;
  static struct option longopts[] = {
    { "footprint",  required_argument,  NULL,  'f' },
    { "installed",  no_argument,        NULL,  'i' },
    { "list",       required_argument,  NULL,  'l' },
    { "owner",      required_argument,  NULL,  'o' },
    { "root",       required_argument,  NULL,  'r' },
    { "version",    no_argument,        NULL,  'V' },
    { "help",       no_argument,        NULL,  'h' },
  };

  while ((opt = getopt_long(argc, argv, "f:il:o:r:Vh", longopts, 0)) != -1)
  {
    switch (opt) {
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

  if (o_footprint_mode + o_installed_mode + o_list_mode + o_owner_mode == 0)
  {
    cerr << "error: option missing" << endl;
    print_help();
    return 1;
  }

  if (o_footprint_mode + o_installed_mode + o_list_mode + o_owner_mode > 1)
  {
    cerr << "error: too many options" << endl;
    print_help();
    return 1;
  }

  pkgutil util("pkginfo");

  if (o_footprint_mode)
  {
    /*
     * Make footprint.
     */
    try
    {
      util.pkg_footprint(o_arg);
    }
    catch (const runtime_error& error)
    {
      cerr << "error: " << error.what() << endl;
      return 1;
    }
  }
  else
  {
    /*
     * Modes that require the database to be opened.
     */
    try
    {
      db_lock lock(o_root, false);
      util.db_open(o_root);
    }
    catch (const runtime_error& error)
    {
      cerr << "error: " << error.what() << endl;
      return 1;
    }

    if (o_installed_mode)
    {
      /*
       * List installed packages.
       */
      for (const auto& package_pair : util.getPackages())
        cout << package_pair.first << ' ' << package_pair.second.version << endl;
    }
    else if (o_list_mode)
    {
      /*
       * List package or file contents.
       */
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
          pair<string, pkgutil::pkginfo_t> package = util.pkg_open(o_arg);
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
        return 1;
      }
    }
    else
    {
      /*
       * List owner(s) of file or directory.
       */
      regex_t preg;
      if (regcomp(&preg, o_arg.c_str(), REG_EXTENDED | REG_NOSUB))
      {
        cerr << "error: fail to compile regular expression '" << o_arg << "', aborting" << endl;
        return 1;
      }

      vector<pair<string, string> > result;
      result.push_back({"Package", "File"});

      unsigned int width = result[0].first.length(); // width of "Package"

      for (const auto& package_pair : util.getPackages())
      {
        for (const string& file : package_pair.second.files)
        {
          const string full_file = '/' + file;

          if (!regexec(&preg, full_file.c_str(), 0, 0, 0))
          {
            result.push_back({package_pair.first, file});

            if (package_pair.first.length() > width)
              width = package_pair.first.length();
          }
        }
      }

      regfree(&preg);

      if (result.size() > 1)
      {
        for (const auto& res_pair : result)
          cout << left << setw(width + 2) << res_pair.first << res_pair.second << endl;
      }
      else
        cout << "pkginfo: no owner(s) found" << endl;
    }
  }

  return 0;
}

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.
