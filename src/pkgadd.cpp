//! \file  pkgadd.cpp
//! \brief pkgadd utility implementation.
//        See COPYING and COPYRIGHT files for corresponding information.

#include <iostream>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <string>
#include <vector>
#include <set>
#include <stdexcept>
#include <cstring> // for strcmp
#include <unistd.h>
#include <getopt.h>
#include <regex.h>

#include "libpkgutils.h" // Include the library header

using namespace std;

// --- Constants for configuration file ---
const string PKGADD_CONF = "/etc/pkgadd.conf";
const int PKGADD_CONF_MAXLINE = 256;

// --- Helper functions from the old pkgadd class ---

struct rule_t {
    enum rule_event_t { INSTALL, UPGRADE };
    rule_event_t event;
    string pattern;
    bool action; // true = YES, false = NO
};

set<string> make_keep_list(const set<string>& files, const vector<rule_t>& rules);
vector<rule_t> read_config(const string& root, const string& configFile);
set<string> apply_install_rules(const string& name, pkgutil::pkginfo_t& info, const vector<rule_t>& rules);


set<string>
make_keep_list(const set<string>&  files, const vector<rule_t>&  rules)
{
    set<string> keep_list;
    vector<rule_t> found;

    auto find_rules = [&](const vector<rule_t>& rules, rule_t::rule_event_t event, vector<rule_t>& found_rules) {
        for (const auto& rule : rules) {
            if (rule.event == event) {
                found_rules.push_back(rule);
            }
        }
    };

    auto rule_applies_to_file = [&](const rule_t& rule, const string& file) {
        regex_t preg;
        bool ret;
        if (regcomp(&preg, rule.pattern.c_str(), REG_EXTENDED | REG_NOSUB)) {
            throw runtime_error("error compiling regular expression '" +
                                rule.pattern + "', aborting");
        }
        ret = !regexec(&preg, file.c_str(), 0, 0, 0);
        regfree(&preg);
        return ret;
    };


    find_rules(rules, rule_t::UPGRADE, found);

    for (const string& file : files) {
        for (auto j = found.rbegin(); j != found.rend(); ++j) {
            if (rule_applies_to_file(*j, file)) {
                if (!(*j).action)
                    keep_list.insert(file);
                break;
            }
        }
    }

#ifndef NDEBUG
    cerr << "Keep list:" << endl;
    for (const string& file : keep_list) {
        cerr << "  " << file << endl;
    }
    cerr << endl;
#endif
    return keep_list;
}


set<string>
apply_install_rules(const string&  name, pkgutil::pkginfo_t&  info, const vector<rule_t>&  rules)
{
    set<string> install_set;
    set<string> non_install_set;
    vector<rule_t> found;

    auto find_rules = [&](const vector<rule_t>& rules, rule_t::rule_event_t event, vector<rule_t>& found_rules) {
        for (const auto& rule : rules) {
            if (rule.event == event) {
                found_rules.push_back(rule);
            }
        }
    };

    auto rule_applies_to_file = [&](const rule_t& rule, const string& file) {
        regex_t preg;
        bool ret;
        if (regcomp(&preg, rule.pattern.c_str(), REG_EXTENDED | REG_NOSUB)) {
            throw runtime_error("error compiling regular expression '" +
                                rule.pattern + "', aborting");
        }
        ret = !regexec(&preg, file.c_str(), 0, 0, 0);
        regfree(&preg);
        return ret;
    };


    find_rules(rules, rule_t::INSTALL, found);

    for (const string& file : info.files) {
        bool install_file = true;
        for (auto j = found.rbegin(); j != found.rend(); ++j) {
            if (rule_applies_to_file(*j, file)) {
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
    for (const string& file : info.files) {
        cerr << "  " << file << endl;
    }
    cerr << endl;

    cerr << "Non-Install set:" << endl;
    for (const string& file : non_install_set) {
        cerr << "  " << file << endl;
    }
    cerr << endl;
#endif
    return non_install_set;
}


vector<rule_t>
read_config(const string& root, const string& configFile)
{
    vector<rule_t> rules;
    unsigned int linecount = 0;
    string filename = root + PKGADD_CONF;
    const int PKGADD_CONF_MAXLINE = 256; // Define max line length here as it's used only in this function

    if (!configFile.empty())
        filename = configFile;

    ifstream in(filename.c_str());

    if (in) {
        while (!in.eof()) {
            string line;
            getline(in, line);
            linecount++;
            if (!line.empty() && line[0] != '#') {
                if (line.length() >= PKGADD_CONF_MAXLINE)
                    throw runtime_error(filename + ":" + std::to_string(linecount) +
                                        ": line too long, aborting");

                char event[PKGADD_CONF_MAXLINE];
                char pattern[PKGADD_CONF_MAXLINE];
                char action[PKGADD_CONF_MAXLINE];
                char dummy[PKGADD_CONF_MAXLINE];

                if (sscanf(line.c_str(), "%s %s %s %s",
                           event, pattern, action, dummy) != 3) {
                    throw runtime_error(filename + ":" + std::to_string(linecount) +
                                        ": wrong number of arguments, aborting");
                }

                if (!strcmp(event, "UPGRADE") || !strcmp(event, "INSTALL")) {
                    rule_t rule;
                    rule.event = strcmp(event, "UPGRADE") ? rule_t::INSTALL : rule_t::UPGRADE;
                    rule.pattern = pattern;

                    if (!strcmp(action, "YES")) {
                        rule.action = true;
                    } else if (!strcmp(action, "NO")) {
                        rule.action = false;
                    } else {
                        throw runtime_error(filename + ":" + std::to_string(linecount) +
                                            ": '" + string(action) +
                                            "' unknown action, should be YES or NO, aborting");
                    }
                    rules.push_back(rule);
                } else {
                    throw runtime_error(filename + ":" + std::to_string(linecount) + ": '" +
                                        string(event) + "' unknown event, aborting");
                }
            }
        }
        in.close();
    }

#ifndef NDEBUG
    cerr << "Configuration:" << endl;
    for (const auto& rule : rules) {
        cerr << "\t" << rule.pattern << "\t" << rule.action << endl;
    }
    cerr << endl;
#endif
    return rules;
}


void print_help()
{
    cout << R"(Usage: pkgadd [-Vfhuv] [-c conffile] [-r rootdir] file
Install software package.

Mandatory arguments to long options are mandatory for short options too.
  -c, --config=conffile    specify an alternate configuration file
  -f, --force              force install, overwrite conflicting files
  -r, --root=rootdir       specify an alternate root directory
  -u, --upgrade            upgrade package with the same name
  -v, --verbose            explain what is being done
  -V, --version            print version and exit
  -h, --help               print help and exit
)";
}

void print_version()
{
    pkgutil util("pkgadd");
    util.print_version();
}


int main(int argc, char** argv) {
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
        { 0,         0,                 0,    0   },
    };

    while ((opt = getopt_long(argc, argv, "c:fr:uvVh", longopts, 0)) != -1) {
        switch (opt) {
            case 'c':
                o_config = optarg;
                break;
            case 'f':
                o_force = 1;
                break;
            case 'r':
                o_root = optarg;
                break;
            case 'u':
                o_upgrade = 1;
                break;
            case 'v':
                o_verbose++;
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

    if (optind == argc) {
        cerr << "Error: missing package name" << endl;
        print_help();
        return 1;
    } else if (argc - optind > 1) {
        cerr << "Error: too many arguments" << endl;
        print_help();
        return 1;
    }

    o_package = argv[optind];

    if (getuid() != 0) {
        cerr << "Error: only root can install/upgrade packages" << endl;
        return 1;
    }

    pkgutil util("pkgadd"); // Create pkgutil object

    try {
        db_lock lock(o_root, true);
        util.db_open(o_root);

        pair<string, pkgutil::pkginfo_t> package   = util.pkg_open(o_package);
        vector<rule_t>  config_rules = read_config(o_root, o_config); // Pass root to read_config

        bool installed = util.db_find_pkg(package.first);

        if (installed && !o_upgrade) {
            throw runtime_error("package " + package.first +
                                " already installed (use -u to upgrade)");
        } else if (!installed && o_upgrade) {
            throw runtime_error("package " + package.first +
                                " not previously installed (skip -u to install)");
        }

        set<string> non_install_files =
            apply_install_rules(package.first, package.second, config_rules);

        set<string> conflicting_files =
            util.db_find_conflicts(package.first, package.second);

        if (!conflicting_files.empty()) {
            if (o_force) {
                set<string> keep_list;
                if (o_upgrade) {
                    /* don't remove files matching the rules in configuration */
                    keep_list = make_keep_list(conflicting_files, config_rules);
                }
                /* remove unwanted conflicts */
                util.db_rm_files(conflicting_files, keep_list); // Call db_rm_files on util
            } else {
                copy(conflicting_files.begin(), conflicting_files.end(),
                     ostream_iterator<string>(cerr, "\n"));

                throw runtime_error("listed file(s) already installed "
                                    "(use -f to ignore and overwrite)");
            }
        }

        set<string> keep_list;
        if (o_upgrade) {
            keep_list = make_keep_list(package.second.files, config_rules);
            util.db_rm_pkg(package.first, keep_list); // Call db_rm_pkg on util
        }

        util.db_add_pkg(package.first, package.second); // Call db_add_pkg on util
        util.db_commit();
        try {
            if (o_verbose)
                cout << (o_upgrade ? "upgrading " : "installing ")
                     << package.first << endl;

            util.pkg_install(o_package, keep_list, non_install_files, installed);
        } catch (runtime_error&) {
            if (!installed) {
                util.db_rm_pkg(package.first); // Call db_rm_pkg on util
                util.db_commit();
                throw runtime_error("failed");
            }
        }
        util.ldconfig(); // Call ldconfig on util

    } catch (const runtime_error& error) {
        cerr << "Error: " << error.what() << endl;
        return 1;
    }

    return 0;
}

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.
