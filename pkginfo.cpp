//! \file  pkginfo.cpp
//! \brief pkginfo utility implementation.
//!        See COPYING and COPYRIGHT files for corresponding information.

#include <iterator>
#include <vector>
#include <iomanip>

#include <sys/types.h>
#include <regex.h>

#include "pkginfo.h"

void pkginfo::print_help() const
{
  cout << "Usage: " << utilname << R"( [OPTION]
Display software package information.

Mandatory arguments to long options are mandatory for short options too.
  -f, --footprint=FILE     print footprint for FILE
  -i, --installed          list installed packages and their version
  -l, --list=PACKAGE|FILE  list files in PACKAGE or FILE
  -o, --owner=PATTERN      list owner(s) of file(s) matching PATTERN
  -r, --root=DIR           specify an alternate installation root
  -V, --version            print version and exit
  -h, --help               print help and exit
)";
}

void pkginfo::run(int argc, char** argv)
{
  /*
   * Check command line options.
   */
  static int o_footprint_mode = 0;
  static int o_installed_mode = 0;
  static int o_list_mode      = 0;
  static int o_owner_mode     = 0;
  static string o_root;
  static string o_arg;
  int opt;
  static struct option longopts[] = {
    { "footprint",  required_argument,  NULL,               'f' },
    { "installed",  no_argument,        NULL,               'i' },
    { "list",       required_argument,  NULL,               'l' },
    { "owner",      required_argument,  NULL,               'o' },
    { "root",       required_argument,  NULL,               'r' },
    { "version",    no_argument,        NULL,               'V' },
    { "help",       no_argument,        NULL,               'h' },
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
      return print_version();
    case 'h':
      return print_help();
    default:
      /* throw an empty message since getopt_long already printed out
       * the error message to stderr */
      throw invalid_argument("");
    }
  }

  if (o_footprint_mode + o_installed_mode + o_list_mode + o_owner_mode == 0)
    throw invalid_argument("option missing");

  if (o_footprint_mode + o_installed_mode + o_list_mode + o_owner_mode > 1)
    throw invalid_argument("too many options");

  if (o_footprint_mode)
  {
    /*
     * Make footprint.
     */
    pkg_footprint(o_arg);
  }
  else
  {
    /*
     * Modes that require the database to be opened.
     */
    {
      db_lock lock(o_root, false);
      db_open(o_root);
    }

    if (o_installed_mode)
    {
      /*
       * List installed packages.
       */
      for (packages_t::const_iterator i = packages.begin();
          i != packages.end(); ++i)
      {
        cout << i->first << ' ' << i->second.version << endl;
      }
    }
    else if (o_list_mode)
    {
      /*
       * List package or file contents.
       */
      if (db_find_pkg(o_arg))
      {
        copy(packages[o_arg].files.begin(),
             packages[o_arg].files.end(),
             ostream_iterator<string>(cout, "\n"));
      }
      else if (file_exists(o_arg))
      {
        pair<string, pkginfo_t> package = pkg_open(o_arg);
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
    else
    {
      /*
       * List owner(s) of file or directory.
       */
      regex_t preg;
      if (regcomp(&preg, o_arg.c_str(), REG_EXTENDED | REG_NOSUB))
      {
        throw runtime_error("error compiling regular expression '" +
            o_arg + "', aborting");
      }

      vector<pair<string, string> > result;
      result.push_back(pair<string, string>("Package", "File"));

      unsigned int width =
        result.begin()->first.length(); /* width of "Package" */

      for (packages_t::const_iterator i = packages.begin();
                                      i != packages.end(); ++i)
      {
        for (set<string>::const_iterator j = i->second.files.begin();
                                         j != i->second.files.end();
                                         ++j)
        {
          const string file('/' + *j);
          if (!regexec(&preg, file.c_str(), 0, 0, 0))
          {
            result.push_back(pair<string, string>(i->first, *j));
            if (i->first.length() > width)
              width = i->first.length();
          }
        }
      }

      regfree(&preg);

      if (result.size() > 1)
      {
        for (vector<pair<string, string>>::const_iterator
              i = result.begin(); i != result.end(); ++i)
        {
          cout << left << setw(width + 2) << i->first << i->second
               << endl;
        }
      }
      else
      {
        cout << utilname << ": no owner(s) found" << endl;
      }
    }
  }
}

// vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
// End of file.
