/*!
 * \file  pkgchk.cpp
 * \brief pkgchk utility implementation.
 *
 * The `pkgchk` utility checks package integrity:
 *   - Symlink integrity (broken links, ownership awareness)
 *   - Disappeared files (missing from filesystem)
 *
 * Options allow running specific checks or a full audit, with
 * verbosity controlling output detail.
 *
 * If no package names are given, all installed packages are checked.
 *
 * \copyright See COPYING for license terms and COPYRIGHT for notices.
 */

#include <iostream>
#include <string>
#include <set>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <libgen.h>
#include <cstring>
#include <regex.h>

#include <libpkgcore/pkgcore.h>

#include <libpkgaudit/auditor.h>
#include <libpkgaudit/probe.h>

#include "pkgutils-config.h"

// Forward declarations
void print_help();
void print_version();

/////////////////////////////////////////////////////////////////////
/// --- Helpers ---                                               ///
/////////////////////////////////////////////////////////////////////

/*!
 * \brief Joins a set of package owner names into a comma-separated
 *        string.
 * \param owners Set of package names.
 * \return Comma-separated string of owners.
 *
 * Used to present ownership awareness in warnings.  If the set is
 * empty, returns "none".
 */
static std::string
join_owners(const std::set<std::string>& owners)
{
  if (owners.empty())
    return "none";

  std::string out;
  bool first = true;

  for (const auto& owner : owners)
  {
    if (!first)
      out += ",";

    out += owner;
    first = false;
  }

  return out;
}

/////////////////////////////////////////////////////////////////////
/// --- CLI ---                                                   ///
/////////////////////////////////////////////////////////////////////

/*!
 * \brief Prints the help message for pkgchk utility to standard
 *        output.
 *
 * Displays usage instructions, available options, and a brief
 * description of the pkgchk utility's functionality.
 */
void
print_help()
{
  std::cout << R"(Usage: pkgchk [-Vh] [-r root-dir]
               {-l | -d | -a} [package-name ...]
Check package integrity.

Mandatory arguments to long options are mandatory for short options too.
  -l, --links              Check symlinks
  -d, --disappeared        Check for disappeared files
  -a, --audit              Run all checks
  -r, --root=root-dir      Use alternate root directory
  -v                       Increase verbosity (repeatable)
  -V, --version            Print version and exit
  -h, --help               Print this help and exit
)";
}

/*!
 * \brief Prints the version information for pkgchk utility to
 *        standard output.
 *
 * Retrieves the version string from the pkgutil library and displays
 * it to the user.
 */
void
print_version()
{
  pkgutil util("pkgchk");
  std::cout << "pkgchk (pkgutils) " << PKGUTILS_VERSION << std::endl;
  util.print_version();
}

/*!
 * \brief Main function for the pkgchk utility.
 * \param argc Argument count from command line.
 * \param argv Argument vector from command line.
 * \return EXIT_SUCCESS on successful execution,
 *         EXIT_FAILURE on error.
 *
 * Parses command line arguments, opens the package database, and runs
 * integrity checks on the specified packages.  If no packages are
 * specified, all installed packages are checked.
 */
int
main(int argc, char** argv)
{
  // --- Option Parsing ---
  int o_links = 0, o_find = 0, o_audit = 0, o_verbose = 0;
  std::string o_root;
  std::vector<std::string> pkgnames;

  static struct option longopts[] = {
    { "links",       no_argument,       NULL, 'l' },
    { "disappeared", no_argument,       NULL, 'd' },
    { "audit",       no_argument,       NULL, 'a' },
    { "root",        required_argument, NULL, 'r' },
    { "version",     no_argument,       NULL, 'V' },
    { "help",        no_argument,       NULL, 'h' },
    { 0,             0,                 0,    0   }
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "ldar:vVh", longopts, 0)) != -1)
  {
    switch (opt)
    {
      case 'l': o_links = 1;      break;
      case 'd': o_find  = 1;      break;
      case 'a': o_audit = 1;      break;
      case 'r': o_root  = optarg; break;
      case 'v': o_verbose++;      break;
      case 'V': print_version();  return EXIT_SUCCESS;
      case 'h': print_help();     return EXIT_SUCCESS;
      default:
        std::cerr << "Invalid option." << std::endl;
        print_help();
        return EXIT_FAILURE;
    }
  }

  // --- Collect package names from remaining args ---
  for (int i = optind; i < argc; i++)
  {
    pkgnames.push_back(argv[i]);
  }

  // --- Argument Validation ---
  int modes = o_links + o_find + o_audit;
  if (modes == 0)
  {
    std::cerr << "pkgchk: option missing\n";
    print_help();
    return EXIT_FAILURE;
  }
  if (modes > 1)
  {
    std::cerr << "pkgchk: too many options\n";
    print_help();
    return EXIT_FAILURE;
  }

  // --- Open Database ---
  pkgutil util("pkgchk");

  try
  {
    db_lock lock(o_root, false);
    util.db_open(o_root);
  }
  catch (const std::runtime_error& e)
  {
    std::cerr << "error: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  // --- If no packages specified, scan all installed ---
  if (pkgnames.empty())
  {
    for (const auto& pkgpair: util.getPackages())
    {
      pkgnames.push_back(pkgpair.first);
    }
  }

  pkgaudit::options opts;
  opts.root = o_root;
  opts.check_links = o_links || o_audit;
  opts.check_disappeared = o_find || o_audit;
  opts.verbosity = o_verbose;

  auto engine = pkgaudit::make_serial_probe_engine();
  pkgaudit::auditor aud(util, *engine);

  for (const auto& pkgname : pkgnames)
  {
    try
    {
      const auto issues = aud.audit_package(pkgname, opts);

      if (opts.check_links)
      {
        std::cout << "Symlink check for " << pkgname << " ...\n";

        for (const auto& issue : issues)
        {
          if (issue.kind != pkgaudit::issue_kind::broken_symlink &&
              issue.kind != pkgaudit::issue_kind::foreign_symlink_target)
            continue;

          if (issue.kind == pkgaudit::issue_kind::broken_symlink)
          {
            std::cout << "ERROR: " << issue.path
                      << " -> " << issue.target
                      << " (broken)\n";
            continue;
          }

          if (issue.kind == pkgaudit::issue_kind::foreign_symlink_target)
          {
            if (opts.verbosity > 0)
            {
              std::cout << "WARNING: " << issue.path
                        << " -> " << issue.target
                        << " (points to " << join_owners(issue.immediate_owners)
                        << ", resolves into " << join_owners(issue.resolved_owners)
                        << ")\n";
            } else {
              std::cout << "WARNING: " << issue.path
                        << " -> " << issue.target << "\n";
            }
          }
        }
      }

      if (opts.check_disappeared)
      {
        std::cout << "Disappeared file check for " << pkgname << " ...\n";

        for (const auto& issue : issues)
        {
          if (issue.kind != pkgaudit::issue_kind::disappeared_file)
            continue;

          std::cout << "ERROR: disappeared file " << issue.path << "\n";

          if (opts.verbosity > 0 && !issue.immediate_owners.empty())
          {
            std::cout << "  Claimed by: " <<
              join_owners(issue.immediate_owners) << "\n";
          }
        }
      }
    }
    catch (const std::runtime_error& e)
    {
      std::cerr << "pkgchk: " << e.what() << "\n";
    }
  }

  return EXIT_SUCCESS;
}
