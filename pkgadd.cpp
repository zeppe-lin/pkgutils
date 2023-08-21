//! \file  pkgadd.cpp
//! \brief pkgadd utility implementation.
//         See COPYING and COPYRIGHT files for corresponding information.

#include <fstream>
#include <iterator>
#include <cstdio>

#include <regex.h>
#include <unistd.h>

#include "pkgadd.h"

set<string> pkgadd::make_keep_list(const set<string>&     files,
                                   const vector<rule_t>&  rules) const
{
  set<string> keep_list;
  vector<rule_t> found;

  find_rules(rules, UPGRADE, found);

  for (set<string>::const_iterator
        i = files.begin(); i != files.end(); i++)
  {
    for (vector<rule_t>::reverse_iterator
          j = found.rbegin(); j != found.rend(); j++)
    {
      if (rule_applies_to_file(*j, *i))
      {
        if (!(*j).action)
          keep_list.insert(keep_list.end(), *i);

        break;
      }
    }
  }

#ifndef NDEBUG
  cerr << "Keep list:" << endl;
  for (set<string>::const_iterator
        j = keep_list.begin(); j != keep_list.end(); j++)
  {
    cerr << "   " << (*j) << endl;
  }
  cerr << endl;
#endif

  return keep_list;
}

void pkgadd::find_rules(const vector<rule_t>&  rules,
                        rule_event_t           event,
                        vector<rule_t>&        found) const
{
  for (vector<rule_t>::const_iterator
        i = rules.begin(); i != rules.end(); i++)
  {
    if (i->event == event)
      found.push_back(*i);
  }
}

bool pkgadd::rule_applies_to_file(const rule_t&  rule,
                                  const string&  file) const
{
  regex_t preg;
  bool ret;

  if (regcomp(&preg, rule.pattern.c_str(), REG_EXTENDED | REG_NOSUB))
    throw runtime_error("error compiling regular expression '" +
                          rule.pattern + "', aborting");

  ret = !regexec(&preg, file.c_str(), 0, 0, 0);
  regfree(&preg);

  return ret;
}

set<string> pkgadd::apply_install_rules(const string&          name,
                                        pkginfo_t&             info,
                                        const vector<rule_t>&  rules)
{
  /* TODO: better algo(?) */
  set<string> install_set;
  set<string> non_install_set;
  vector<rule_t> found;

  find_rules(rules, INSTALL, found);

  for (set<string>::const_iterator
        i = info.files.begin(); i != info.files.end(); i++)
  {
    bool install_file = true;

    for (vector<rule_t>::reverse_iterator
          j = found.rbegin(); j != found.rend(); j++)
    {
      if (rule_applies_to_file(*j, *i))
      {
        install_file = (*j).action;
        break;
      }
    }

    if (install_file)
      install_set.insert(install_set.end(), *i);
    else
      non_install_set.insert(*i);
  }

  info.files.clear();
  info.files = install_set;

#ifndef NDEBUG
  cerr << "Install set:" << endl;
  for (set<string>::iterator
        j = info.files.begin(); j != info.files.end(); j++)
  {
    cerr << "   " << (*j) << endl;
  }
  cerr << endl;

  cerr << "Non-Install set:" << endl;
  for (set<string>::iterator
        j = non_install_set.begin(); j != non_install_set.end(); j++)
  {
    cerr << "   " << (*j) << endl;
  }
  cerr << endl;
#endif

  return non_install_set;
}

vector<rule_t> pkgadd::read_config(string file) const
{
  vector<rule_t> rules;
  unsigned int linecount = 0;
  string filename = root + PKGADD_CONF;

  if (!file.empty())
    filename = file;

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
          throw runtime_error(filename + ":" + itos(linecount) +
              ": line too long, aborting");

        char event[PKGADD_CONF_MAXLINE];
        char pattern[PKGADD_CONF_MAXLINE];
        char action[PKGADD_CONF_MAXLINE];
        char dummy[PKGADD_CONF_MAXLINE];

        if (sscanf(line.c_str(), "%s %s %s %s",
              event, pattern, action, dummy) != 3)
        {
          throw runtime_error(filename + ":" + itos(linecount) +
              ": wrong number of arguments, aborting");
        }

        if (!strcmp(event, "UPGRADE") || !strcmp(event, "INSTALL"))
        {
          rule_t rule;
          rule.event = strcmp(event, "UPGRADE") ? INSTALL : UPGRADE;
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
            throw runtime_error(filename + ":" + itos(linecount) +
                ": '" + string(action) +
                "' unknown action, should be YES or NO, aborting");
          }

          rules.push_back(rule);
        }
        else
        {
          throw runtime_error(filename + ":" + itos(linecount) + ": '" +
                  string(event) + "' unknown event, aborting");
        }
      }
    }
    in.close();
  }

#ifndef NDEBUG
  cerr << "Configuration:" << endl;
  for (vector<rule_t>::const_iterator j = rules.begin(); j != rules.end(); j++)
  {
    cerr << "\t" << (*j).pattern << "\t" << (*j).action << endl;
  }
  cerr << endl;
#endif

  return rules;
}

void pkgadd::print_help() const
{
  cout << "Usage: " << utilname << R"( [OPTION]... FILE
Install software package.

Mandatory arguments to long options are mandatory for short options too.
  -u, --upgrade        upgrade package with the same name
  -f, --force          force install, overwrite conflicting files
  -r, --root=PATH      specify alternative installation root
  -c, --config=FILE    use alternative configuration file
  -v, --verbose        explain what is being done
  -V, --version        print version and exit
  -h, --help           print help and exit
)";
}

void pkgadd::run(int argc, char** argv)
{
  /*
   * Check command line options.
   */
  static int o_upgrade = 0, o_force = 0, o_verbose = 0;
  static string o_root, o_config = PKGADD_CONF, o_package;
  int opt;
  static struct option longopts[] = {
    { "root",     required_argument,  NULL,           'r' },
    { "config",   required_argument,  NULL,           'c' },
    { "upgrade",  no_argument,        NULL,           'u' },
    { "force",    no_argument,        NULL,           'f' },
    { "verbose",  no_argument,        NULL,           'v' },
    { "version",  no_argument,        NULL,           'V' },
    { "help",     no_argument,        NULL,           'h' },
    { 0,          0,                  0,              0   },
  };

  while ((opt = getopt_long(argc, argv, "r:c:ufvVh", longopts, 0)) != -1)
  {
    char ch = static_cast<char>(optopt);
    switch (opt) {
    case 'r':
      o_root = optarg;
      break;
    case 'c':
      o_config = optarg;
      break;
    case 'u':
      o_upgrade = 1;
      break;
    case 'f':
      o_force = 1;
      break;
    case 'v':
      o_verbose++;
      break;
    case 'V':
      return print_version();
    case 'h':
      return print_help();
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
    throw runtime_error("only root can install/upgrade packages");

  /*
   * Install or upgrade package.
   */
  {
    db_lock lock(o_root, true);
    db_open(o_root);

    pair<string, pkginfo_t> package      = pkg_open(o_package);
    vector<rule_t>          config_rules = read_config(o_config);

    bool installed = db_find_pkg(package.first);

    if (installed && !o_upgrade)
      throw runtime_error("package " + package.first +
                          " already installed (use -u to upgrade)");

    else if (!installed && o_upgrade)
      throw runtime_error("package " + package.first +
                          " not previously installed (skip -u to install)");

    set<string> non_install_files =
      apply_install_rules(package.first, package.second, config_rules);

    set<string> conflicting_files =
      db_find_conflicts(package.first, package.second);

    if (!conflicting_files.empty())
    {
      if (o_force)
      {
        set<string> keep_list;
        if (o_upgrade)
        {
          /* don't remove files matching the rules in configuration */
          keep_list = make_keep_list(conflicting_files, config_rules);
        }
        /* remove unwanted conflicts */
        db_rm_files(conflicting_files, keep_list);
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
      db_rm_pkg(package.first, keep_list);
    }

    db_add_pkg(package.first, package.second);
    db_commit();
    try
    {
      if (o_verbose)
        cout << (o_upgrade ? "upgrading " : "installing ")
             << package.first << endl;

      pkg_install(o_package, keep_list, non_install_files, installed);
    }
    catch (runtime_error&)
    {
      if (!installed)
      {
        db_rm_pkg(package.first);
        db_commit();
        throw runtime_error("failed");
      }
    }
    ldconfig();
  }
}

// vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
// End of file.
