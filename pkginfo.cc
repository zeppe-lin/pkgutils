// This file is a part of pkgutils.
// See COPYING and COPYRIGHT files for corresponding information.

#include "pkginfo.h"
#include <iterator>
#include <vector>
#include <iomanip>
#include <sys/types.h>
#include <regex.h>

void pkginfo::run(int argc, char** argv)
{
  //
  // Check command line options
  //
  int     o_footprint_mode = 0;
  int     o_installed_mode = 0;
  int     o_list_mode      = 0;
  int     o_owner_mode     = 0;
  string  o_root;
  string  o_arg;

  for (int i = 1; i < argc; ++i)
  {
    string option(argv[i]);

    if (option == "-r" || option == "--root")
    {
      assert_argument(argv, argc, i);
      o_root = argv[i + 1];
      i++;
    }
    else if (option == "-i" || option == "--installed")
    {
      o_installed_mode += 1;
    }
    else if (option == "-l" || option == "--list")
    {
      assert_argument(argv, argc, i);
      o_list_mode += 1;
      o_arg = argv[i + 1];
      i++;
    }
    else if (option == "-o" || option == "--owner")
    {
      assert_argument(argv, argc, i);
      o_owner_mode += 1;
      o_arg = argv[i + 1];
      i++;
    }
    else if (option == "-f" || option == "--footprint")
    {
      assert_argument(argv, argc, i);
      o_footprint_mode += 1;
      o_arg = argv[i + 1];
      i++;
    }
    else
    {
      throw runtime_error("invalid option " + option);
    }
  }

  if (o_footprint_mode + o_installed_mode + o_list_mode + o_owner_mode == 0)
    throw runtime_error("option missing");

  if (o_footprint_mode + o_installed_mode + o_list_mode + o_owner_mode > 1)
    throw runtime_error("too many options");

  if (o_footprint_mode)
  {
    //
    // Make footprint
    //
    pkg_footprint(o_arg);
  }
  else
  {
    //
    // Modes that require the database to be opened
    //
    {
      db_lock lock(o_root, false);
      db_open(o_root);
    }

    if (o_installed_mode)
    {
      //
      // List installed packages
      //
      for (packages_t::const_iterator i = packages.begin();
          i != packages.end(); ++i)
      {
        cout << i->first << ' ' << i->second.version << endl;
      }
    }
    else if (o_list_mode)
    {
      //
      // List package or file contents
      //
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
      //
      // List owner(s) of file or directory
      //
      regex_t preg;
      if (regcomp(&preg, o_arg.c_str(), REG_EXTENDED | REG_NOSUB))
      {
        throw runtime_error("error compiling regular expression '" +
            o_arg + "', aborting");
      }

      vector<pair<string, string> > result;
      result.push_back(pair<string, string>("Package", "File"));

      unsigned int width =
        result.begin()->first.length(); // Width of "Package"

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
        for (vector<pair<string, string> >::const_iterator i = result.begin(); i != result.end(); ++i)
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

void pkginfo::print_help() const
{
  cout
    << "Usage: " << utilname << " [OPTION]"                                       << endl
    << "Display software package information."                                    << endl
                                                                                  << endl
    << "Mandatory arguments to long options are mandatory for short options too." << endl
    << "  -i, --installed             list installed packages"                    << endl
    << "  -l, --list PACKAGE|FILE     list files in PACKAGE or FILE"              << endl
    << "  -o, --owner PATTERN         list owner(s) of file(s) matching PATTERN"  << endl
    << "  -f, --footprint FILE        print footprint for FILE"                   << endl
    << "  -r, --root PATH             specify alternative installation root"      << endl
    << "  -v, --version               print version and exit"                     << endl
    << "  -h, --help                  print help and exit"                        << endl;
}

// vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
// End of file.
