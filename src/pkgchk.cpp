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

#include "libpkgutils.h" // Include the library header

// Forward declarations
void check_links(pkgutil&, const std::string&, const std::string&, int);
void check_disappeared(pkgutil&, const std::string&, const std::string&, int);
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

  for (auto it = owners.begin(); it != owners.end(); ++it)
  {
    if (it != owners.begin())
      out += ",";

    out += *it;
  }

  return out;
}

/*!
 * \brief Escape regex metacharacters in a filesystem path.
 * \param path Path string to escape.
 * \return Escaped regex string.
 *
 * Ensures that literal paths can be safely compiled into regexes for
 * ownership lookup.
 */
static std::string
escape_regex(const std::string& path)
{
  std::string out;

  for (char c : path)
  {
    if (strchr(".[]*^$()+?{|}", c))
      out.push_back('\\');

    out.push_back(c);
  }

  return out;
}

/*!
 * \brief Find packages that own files matching a regex pattern.
 * \param util    pkgutil instance with open database.
 * \param pattern Regex pattern to match against file paths.
 * \return Set of package names owning matching files.
 *
 * Iterates over the package database and collects all packages whose
 * file list matches the given regex.
 */
static std::set<std::string>
find_owners(pkgutil& util, const std::string& pattern)
{
  std::set<std::string> owners;
  regex_t preg;

  if (regcomp(&preg, pattern.c_str(), REG_EXTENDED | REG_NOSUB))
    return owners;

  for (const auto& pkgpair : util.getPackages())
  {
    for (const auto& file : pkgpair.second.files)
    {
      std::string full = "/" + file;

      if (!regexec(&preg, full.c_str(), 0, 0, 0))
        owners.insert(pkgpair.first);
    }
  }

  regfree(&preg);

  return owners;
}

/////////////////////////////////////////////////////////////////////
/// --- Symlink check ---                                         ///
/////////////////////////////////////////////////////////////////////

/*!
 * \brief Check symlink integrity for a package.
 * \param util      pkgutil instance with open database.
 * \param pkgname   Package name to check.
 * \param root      Root directory prefix for filesystem checks.
 * \param verbosity Verbosity level (0 = errors only, >0 = warnings).
 *
 * Reports broken symlinks as errors.  At verbosity > 0, also reports
 * ownership awareness: whether the symlink target belongs to another
 * package.  At verbosity > 1, prints detailed owner lists.
 */
void
check_links(pkgutil& util, const std::string& pkgname,
            const std::string& root, int verbosity)
{
  auto pkgs = util.getPackages();
  auto it = pkgs.find(pkgname);
  if (it == pkgs.end())
  {
    std::cerr << "pkgchk: package not found: " << pkgname << "\n";
    return;
  }

  std::cout << "Symlink check for " << pkgname << "...\n";

  for (const auto& path : it->second.files)
  {
    std::string full = root + "/" + path;
    struct stat st;

    if (lstat(full.c_str(), &st) == -1)
      continue;

    if (!S_ISLNK(st.st_mode))
      continue;

    char buf[PATH_MAX];
    ssize_t len = readlink(full.c_str(), buf, sizeof(buf)-1);
    if (len == -1)
      continue;
    buf[len] = '\0';
    std::string target(buf);

    std::string immediate;
    if (target[0] == '/')
    {
      immediate = root + target;
    }
    else
    {
      char* dup = strdup(full.c_str());
      immediate = std::string(dirname(dup)) + "/" + target;
      free(dup);
    }
    immediate = trim_filename(immediate);

    if (!file_exists(immediate))
    {
      std::cout << "ERROR: " << full << " -> " << target << " (broken)\n";
      continue;
    }

    auto immOwners = find_owners(util, escape_regex(immediate));
    char* resolvedPath = realpath(immediate.c_str(), nullptr);
    std::string resolved = resolvedPath ? resolvedPath : immediate;
    free(resolvedPath);
    auto finOwners = find_owners(util, escape_regex(resolved));

    if (immOwners.count(pkgname) || finOwners.count(pkgname))
      continue;

    if (verbosity > 0)
    {
      std::cout << "WARNING: " << full << " -> " << target
                << " (points to " << join_owners(immOwners)
                << ", resolves into " << join_owners(finOwners) << ")\n";
    }
    else
    {
      std::cout << "WARNING: " << full << " -> " << target << "\n";
    }
  }
}

/////////////////////////////////////////////////////////////////////
/// --- Disappeared file check ---                                ///
/////////////////////////////////////////////////////////////////////

/*!
 * \brief Check for disappeared files in a package.
 * \param util      pkgutil instance with open database.
 * \param pkgname   Package name to check.
 * \param root      Root directory prefix for filesystem checks.
 * \param verbosity Verbosity level
 *                  (0 = errors only, >0 = ownership info).
 *
 * Reports files listed in the package database that no longer exist
 * under the filesystem.  At verbosity > 0, also reports which
 * packages still claim ownership of the missing file.
 */
void
check_disappeared(pkgutil& util, const std::string& pkgname,
                  const std::string& root, int verbosity)
{
  auto pkgs = util.getPackages();
  auto it = pkgs.find(pkgname);
  if (it == pkgs.end())
  {
    std::cerr << "pkgchk: package not found: " << pkgname << "\n";
    return;
  }

  std::cout << "Disappeared file check for " << pkgname << "...\n";

  for (const auto& path : it->second.files)
  {
    std::string full = root + "/" + path;

    if (file_exists(full))
      continue;

    std::cout << "ERROR: disappeared file " << full << "\n";

    if (verbosity > 0)
    {
      std::set<std::string> owners;
      for (const auto& pkgpair : pkgs)
      {
        if (pkgpair.second.files.count(path))
        {
          owners.insert(pkgpair.first);
        }
      }
      if (!owners.empty())
      {
        std::cout << "  Claimed by: " << join_owners(owners) << "\n";
      }
    }
  }
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

  // --- Run checks for each package ---
  for (const auto& pkgname : pkgnames)
  {
    if (o_links || o_audit)
    {
      check_links(util, pkgname, o_root, o_verbose);
    }
    if (o_find || o_audit)
    {
      check_disappeared(util, pkgname, o_root, o_verbose);
    }
  }

  return EXIT_SUCCESS;
}
