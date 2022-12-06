// See COPYING and COPYRIGHT files for corresponding information.

#include "pkgrm.h"
#include <unistd.h>

void pkgrm::run(int argc, char** argv)
{
  //
  // Check command line options
  //
  string o_package;
  string o_root;

  for (int i = 1; i < argc; i++)
  {
    string option(argv[i]);

    if (option == "-r" || option == "--root")
    {
      assert_argument(argv, argc, i);
      o_root = argv[i + 1];
      i++;
    }
    else if (option[0] == '-' || !o_package.empty())
    {
      throw runtime_error("invalid option " + option);
    }
    else
    {
      o_package = option;
    }
  }

  if (o_package.empty())
    throw runtime_error("option missing");

  //
  // Check UID
  //
  if (getuid())
    throw runtime_error("only root can remove packages");

  //
  // Remove package
  //
  {
    db_lock lock(o_root, true);
    db_open(o_root);

    if (!db_find_pkg(o_package))
      throw runtime_error("package " + o_package + " not installed");

    db_rm_pkg(o_package);
    ldconfig();
    db_commit();
  }
}

void pkgrm::print_help() const
{
  cout
    << "Usage: " << utilname << " [OPTION] PKGNAME"                               << endl
    << "Remove software package."                                                 << endl
                                                                                  << endl
    << "Mandatory arguments to long options are mandatory for short options too." << endl
    << "  -r, --root <path>   specify alternative installation root"              << endl
    << "  -v, --version       print version and exit"                             << endl
    << "  -h, --help          print help and exit"                                << endl;
}

// vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
// End of file.
