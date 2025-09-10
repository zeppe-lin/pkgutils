//! \file  pkgadd.cpp
//! \brief pkgadd utility implementation.
//!
//! The `pkgadd` utility is used to install software packages.
//! It handles package installation, upgrades, conflict detection,
//! and rule-based file management based on configuration files.
//!
//! \copyright See COPYING and COPYRIGHT files for corresponding
//!            information.

#include <iostream>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <string>
#include <vector>
#include <set>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <getopt.h>
#include <regex.h>

#include "libpkgutils.h" // Include the library header

using namespace std;

//////////////////////////////////////////////////////////////////////
/// --- Constants ---                                              ///
//////////////////////////////////////////////////////////////////////

//! \def PKGADD_CONF
//! \brief Default location for pkgadd configuration file.
const string PKGADD_CONF = "/etc/pkgadd.conf";

//! \def PKGADD_CONF_MAXLINE
//! \brief Default max length for configuration statement.
const int PKGADD_CONF_MAXLINE = 256;


//////////////////////////////////////////////////////////////////////
/// --- Rule structure ---                                         ///
//////////////////////////////////////////////////////////////////////

/*!
 * \struct rule_t
 * \brief Structure to represent a rule from the configuration file.
 *
 * This structure defines a rule that specifies actions to be taken
 * during package installation or upgrade based on file patterns.
 */
struct rule_t {
    //! \enum rule_event_t
    //! \brief Events that trigger a rule.
    enum rule_event_t {
      INSTALL, //!< Rule triggered during package installation.
      UPGRADE  //!< Rule triggered during package upgrade.
    };

    //! \var event
    //! \brief The event that triggers the rule (INSTALL or UPGRADE).
    rule_event_t event;

    //! \var pattern
    //! \brief Regular expression pattern to match against file paths.
    string pattern;

    //! \var action
    //! \brief Action to take when the rule applies.
    //!
    //!  - `true` (YES):  Perform the action (e.g., install file).
    //!  - `false` (NO):  Do not perform the action (e.g., keep file).
    bool action;
};


//////////////////////////////////////////////////////////////////////
/// --- Helper Function Declarations ---                           ///
//////////////////////////////////////////////////////////////////////

/*!
 * \brief Creates a list of files to keep during package upgrade.
 * \param files Set of conflicting files.
 * \param rules Vector of configuration rules.
 * \return Set of files to keep (not remove) during upgrade.
 */
set<string> make_keep_list(const set<string>& files,
                           const vector<rule_t>& rules);

/*!
 * \brief Reads configuration rules from the pkgadd configuration file.
 * \param root Root directory for installation (used to locate config).
 * \param configFile Optional path to an alternate configuration file.
 * \return Vector of configuration rules read from the file.
 */
vector<rule_t> read_config(const string& root,
                           const string& configFile);

/*!
 * \brief Applies install rules to filter files for installation.
 * \param name Package name.
 * \param info Package information structure containing file list.
 * \param rules Vector of configuration rules.
 * \return Set of files that were *not* installed (excluded by rules).
 */
set<string> apply_install_rules(const string& name,
                                pkgutil::pkginfo_t& info,
                                const vector<rule_t>& rules);

/*!
 * \brief Prints the help message for pkgadd utility.
 */
void print_help();

/*!
 * \brief Prints the version information for pkgadd utility.
 */
void print_version();


//////////////////////////////////////////////////////////////////////
/// --- Helper Function Implementations ---                        ///
//////////////////////////////////////////////////////////////////////

/*!
 * \brief Creates a list of files to keep during package upgrade.
 * \param files Set of conflicting files.
 * \param rules Vector of configuration rules.
 * \return Set of files to keep (not remove) during upgrade.
 *
 * This function iterates through the conflicting files and applies
 * the 'UPGRADE' rules from the configuration. If a rule matches a
 * file and its action is 'NO' (false), the file is added to the
 * keep list.
 */
set<string>
make_keep_list(const set<string>& files,
               const vector<rule_t>& rules)
{
  set<string> keep_list;
  vector<rule_t> found;

  /*!
   * \brief Local lambda function to find rules for a given event.
   * \param rules Vector of all rules.
   * \param event Event type to search for (INSTALL or UPGRADE).
   * \param found_rules Output vector to store found rules.
   */
  auto find_rules = [&](const vector<rule_t>& rule_list,
                        rule_t::rule_event_t event,
                        vector<rule_t>& found_rules)
  {
    for (const auto& rule : rule_list)
    {
      if (rule.event == event)
        found_rules.push_back(rule);
    }
  };

  /*!
   * \brief Local lambda function to check if a rule applies to a file.
   * \param rule Configuration rule.
   * \param file File path to check against the rule.
   * \return True if the rule's pattern matches the file path, false otherwise.
   */
  auto rule_applies_to_file = [&](const rule_t& rule, const string& file)
  {
    regex_t preg;
    bool ret;
    if (regcomp(&preg, rule.pattern.c_str(),
                REG_EXTENDED | REG_NOSUB))
    {
        throw runtime_error("error compiling regular "
                            "expression '" +
                            rule.pattern + "', aborting");
    }
    ret = !regexec(&preg, file.c_str(), 0, 0, 0);
    regfree(&preg);
    return ret;
  };

  find_rules(rules, rule_t::UPGRADE, found);

  for (const string& file : files)
  {
    for (auto j = found.rbegin(); j != found.rend(); ++j)
    {
      if (rule_applies_to_file(*j, file))
      {
        if (!(*j).action)
            keep_list.insert(file);
        break;
      }
    }
  }

#ifndef NDEBUG
  cerr << "Keep list:" << endl;
  for (const string& file : keep_list)
    cerr << "  " << file << endl;
  cerr << endl;
#endif
  return keep_list;
}


/*!
 * \brief Applies install rules to filter files for installation.
 * \param name Package name.
 * \param info Package information structure containing file list.
 * \param rules Vector of configuration rules.
 * \return Set of files that were *not* installed (excluded by rules).
 *
 * This function iterates through the files in the package information
 * and applies 'INSTALL' rules from the configuration. If a rule
 * matches a file, the rule's action (YES/NO) determines if the
 * file is included in the install set or excluded (non-install set).
 */
set<string>
apply_install_rules(const string& name,
                    pkgutil::pkginfo_t& info,
                    const vector<rule_t>& rules)
{
  set<string> install_set;
  set<string> non_install_set;
  vector<rule_t> found;

  /*!
   * \brief Local lambda function to find rules for a given event.
   * \param rules Vector of all rules.
   * \param event Event type to search for (INSTALL or UPGRADE).
   * \param found_rules Output vector to store found rules.
   */
  auto find_rules = [&](const vector<rule_t>& rule_list,
                        rule_t::rule_event_t event,
                        vector<rule_t>& found_rules)
  {
    for (const auto& rule : rule_list)
    {
      if (rule.event == event)
        found_rules.push_back(rule);
    }
  };

  /*!
   * \brief Local lambda function to check if a rule applies to a file.
   * \param rule Configuration rule.
   * \param file File path to check against the rule.
   * \return True if the rule's pattern matches the file path, false otherwise.
   */
  auto rule_applies_to_file = [&](const rule_t& rule,
                                  const string& file)
  {
    regex_t preg;
    bool ret;
    if (regcomp(&preg, rule.pattern.c_str(),
                REG_EXTENDED | REG_NOSUB))
    {
      throw runtime_error("error compiling regular "
                          "expression '" +
                          rule.pattern + "', aborting");
    }
    ret = !regexec(&preg, file.c_str(), 0, 0, 0);
    regfree(&preg);
    return ret;
  };

  find_rules(rules, rule_t::INSTALL, found);

  for (const string& file : info.files)
  {
    bool install_file = true;
    for (auto j = found.rbegin(); j != found.rend(); ++j)
    {
      if (rule_applies_to_file(*j, file))
      {
        install_file = (*j).action;
        break;
      }
    }
    if (install_file)
      install_set.insert(file);
    else
      non_install_set.insert(file);
  }

  info.files.clear();
  info.files = install_set;

#ifndef NDEBUG
  cerr << "Install set:" << endl;
  for (const string& file : info.files)
    cerr << "  " << file << endl;
  cerr << endl;

  cerr << "Non-Install set:" << endl;
  for (const string& file : non_install_set)
    cerr << "  " << file << endl;
  cerr << endl;
#endif
  return non_install_set;
}


/*!
 * \brief Reads configuration rules from the pkgadd configuration file.
 * \param root Root directory for installation (used to locate config).
 * \param configFile Optional path to an alternate configuration file.
 * \return Vector of configuration rules read from the file.
 *
 * This function reads the configuration file line by line, parsing
 * each non-empty, non-comment line into a rule_t structure.
 * Each rule specifies an event (INSTALL/UPGRADE), a regular
 * expression pattern, and an action (YES/NO).
 */
vector<rule_t>
read_config(const string& root, const string& configFile)
{
  vector<rule_t> rules;
  unsigned int linecount = 0;
  string filename = root + PKGADD_CONF;

  if (!configFile.empty())
    filename = configFile;

  ifstream in(filename.c_str());

  if (in)
  {
    while (!in.eof())
    {
      string line;
      getline(in, line);
      linecount++;
      if (!line.empty() && line[0] != '#')
      {
        if (line.length() >= PKGADD_CONF_MAXLINE)
          throw runtime_error(filename + ":" +
                              std::to_string(linecount) +
                              ": line too long, aborting");

        char event[PKGADD_CONF_MAXLINE];
        char pattern[PKGADD_CONF_MAXLINE];
        char action[PKGADD_CONF_MAXLINE];
        char dummy[PKGADD_CONF_MAXLINE];

        if (sscanf(line.c_str(), "%s %s %s %s",
                   event, pattern, action, dummy) != 3)
        {
          throw runtime_error(filename + ":" +
                              std::to_string(linecount) +
                              ": wrong number of arguments,"
                              " aborting");
        }

        if (!strcmp(event, "UPGRADE") ||
            !strcmp(event, "INSTALL"))
        {
          rule_t rule;
          rule.event = strcmp(event, "UPGRADE") ?
                          rule_t::INSTALL : rule_t::UPGRADE;
          rule.pattern = pattern;

          if (!strcmp(action, "YES"))
          {
            rule.action = true;
          }
          else if (!strcmp(action, "NO"))
          {
            rule.action = false;
          }
          else
          {
            throw runtime_error(filename + ":" +
                                std::to_string(linecount) +
                                ": '" + string(action) +
                                "' unknown action, should be"
                                " YES or NO, aborting");
          }

          rules.push_back(rule);
        }
        else
        {
          throw runtime_error(filename + ":" +
                              std::to_string(linecount) +
                              ": '" + string(event) +
                              "' unknown event, aborting");
        }
      }
    } // end while (!eof())
    in.close();
  } // end if (in)

#ifndef NDEBUG
  cerr << "Configuration:" << endl;
  for (const auto& rule : rules)
    cerr << "\t" << rule.pattern << "\t" << rule.action << endl;
  cerr << endl;
#endif
  return rules;
}


/*!
 * \brief Prints the help message for pkgadd utility to standard output.
 *
 * Displays usage instructions, available options, and a brief
 * description of the pkgadd utility's functionality.
 */
void
print_help()
{
  cout << R"(Usage: pkgadd [-Vfhuv] [-c config-file] [-r root-dir] package-archive
Install or upgrade a software package.

Mandatory arguments to long options are mandatory for short options too.
  -c, --config=config-file   Use an alternate configuration file
  -r, --root=root-dir        Use an alternate root directory
  -u, --upgrade              Upgrade the installed package
  -f, --force                Force install, overwrite conflicting files
  -v, --verbose              Explain what is being done
  -V, --version              Print version and exit
  -h, --help                 Print this message and exit
)";
}


/*!
 * \brief Prints the version information for pkgadd utility to standard output.
 *
 * Retrieves the version string from the pkgutil library and displays
 * it to the user.
 */
void
print_version()
{
  pkgutil util("pkgadd");
  util.print_version();
}


/*!
 * \brief Main function for the pkgadd utility.
 * \param argc Argument count from command line.
 * \param argv Argument vector from command line.
 * \return EXIT_SUCCESS on successful execution, EXIT_FAILURE on error.
 *
 * This function parses command line arguments, performs package
 * installation or upgrade based on options and configuration rules,
 * and handles error conditions.
 */
int
main(int argc, char** argv)
{
  // --- Option Parsing ---
  int o_upgrade = 0, o_force = 0, o_verbose = 0;
  string o_root, o_config = PKGADD_CONF;
  string o_package;
  int opt;

  static struct option longopts[] = {
    { "config",  required_argument, NULL, 'c' },
    { "force",   no_argument,       NULL, 'f' },
    { "root",    required_argument, NULL, 'r' },
    { "upgrade", no_argument,       NULL, 'u' },
    { "verbose", no_argument,       NULL, 'v' },
    { "version", no_argument,       NULL, 'V' },
    { "help",    no_argument,       NULL, 'h' },
    { 0,         0,                 0,    0   }
  };

  while ((opt = getopt_long(argc, argv, "c:fr:uvVh", longopts, 0)) != -1)
  {
    switch (opt)
    {
      case 'c': o_config  = optarg; break;
      case 'f': o_force   = 1;      break;
      case 'r': o_root    = optarg; break;
      case 'u': o_upgrade = 1;      break;
      case 'v': o_verbose++;        break;
      case 'V': print_version();    return EXIT_SUCCESS;
      case 'h': print_help();       return EXIT_SUCCESS;
      default:
        cerr << "Invalid option." << endl;
        print_help();
        return EXIT_FAILURE;
    }
  }

  // --- Argument Validation ---
  if (optind == argc)
  {
    cerr << "Error: missing package name" << endl;
    print_help();
    return EXIT_FAILURE;
  }
  else if (argc - optind > 1)
  {
    cerr << "Error: too many arguments" << endl;
    print_help();
    return EXIT_FAILURE;
  }

  o_package = argv[optind];

  // --- Privilege Check ---
  if (getuid() != 0)
  {
    cerr << "Error: only root can install/upgrade packages" << endl;
    return EXIT_FAILURE;
  }

  // --- Package Installation/Upgrade Logic ---
  pkgutil util("pkgadd");

  try
  {
    db_lock lock(o_root, true);
    util.db_open(o_root);

    pair<string, pkgutil::pkginfo_t> package = util.pkg_open(o_package);

    vector<rule_t> config_rules = read_config(o_root, o_config);
    bool installed = util.db_find_pkg(package.first);

    if (installed && !o_upgrade)
    {
      throw runtime_error("package " + package.first +
                          " already installed (use -u to upgrade)");
    }
    else if (!installed && o_upgrade)
    {
      throw runtime_error("package " + package.first +
                          " not previously installed (skip -u to install)");
    }

    set<string> non_install_files =
      apply_install_rules(package.first, package.second, config_rules);

    set<string> conflicting_files =
      util.db_find_conflicts(package.first, package.second);

    if (!conflicting_files.empty())
    {
      if (o_force)
      {
        set<string> keep_list;

        if (o_upgrade)
          keep_list = make_keep_list(conflicting_files, config_rules);

        util.db_rm_files(conflicting_files, keep_list);
      }
      else
      {
        copy(conflicting_files.begin(), conflicting_files.end(),
             ostream_iterator<string>(cerr, "\n"));
        throw runtime_error("listed file(s) already installed "
                            "(use -f to ignore and overwrite)");
      }
    }

    set<string> keep_list;
    if (o_upgrade)
    {
      keep_list = make_keep_list(package.second.files, config_rules);
      util.db_rm_pkg(package.first, keep_list);
    }

    util.db_add_pkg(package.first, package.second);
    util.db_commit();

    if (o_verbose)
    {
      cout << (o_upgrade ? "upgrading " : "installing ")
           << package.first << endl;
    }

    util.pkg_install(o_package, keep_list, non_install_files, o_upgrade);

  }
  catch (const runtime_error& error)
  {
    cerr << "Error: " << error.what() << endl;
    return EXIT_FAILURE;
  }

  util.ldconfig();
  return EXIT_SUCCESS;
}

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.
