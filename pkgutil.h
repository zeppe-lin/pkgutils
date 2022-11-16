// This file is a part of pkgutils.
// See COPYING and COPYRIGHT files for corresponding information.

#ifndef PKGUTIL_H
#define PKGUTIL_H

#include <string>
#include <set>
#include <map>
#include <iostream>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <dirent.h>

#define PKG_EXT         ".pkg.tar."
#define PKG_DIR         "var/lib/pkg"
#define PKG_DB          "var/lib/pkg/db"
#define PKG_REJECTED    "var/lib/pkg/rejected"
#define VERSION_DELIM   '#'
#define LDCONFIG        "/sbin/ldconfig"
#define LDCONFIG_CONF   "/etc/ld.so.conf"

using namespace std;

class pkgutil
{
public:
  struct pkginfo_t
  {
    string      version;
    set<string> files;
  };

  typedef map<string, pkginfo_t> packages_t;

  explicit pkgutil(const string& name);

  virtual ~pkgutil() {}

  virtual void run(int argc, char** argv) = 0;

  virtual void print_help() const = 0;

  void print_version() const;

protected:
  // Database
  void db_open(const string& path);

  void db_commit();

  void db_add_pkg(const string&    name,
                  const pkginfo_t& info);

  bool db_find_pkg(const string& name);
  void db_rm_pkg(const string& name);

  void db_rm_pkg(const string&       name,
                 const set<string>&  keep_list);

  void db_rm_files(set<string>         files,
                   const set<string>&  keep_list);

  set<string> db_find_conflicts(const string&     name,
                                const pkginfo_t&  info);

  // Tar.gz
  pair<string, pkginfo_t> pkg_open(const string& filename) const;

  void pkg_install(const string&       filename,
                   const set<string>&  keep_list,
                   const set<string>&  non_install_files,
                   bool                upgrade) const;

  void pkg_footprint(string& filename) const;

  void ldconfig() const;

  string utilname;

  packages_t packages;

  string root;
};

class db_lock
{
public:
  db_lock(const string&  root,
          bool           exclusive);

  ~db_lock();
private:
  DIR* dir;
};

class runtime_error_with_errno : public runtime_error
{
public:
  explicit runtime_error_with_errno(const string& msg) throw()
    : runtime_error(msg + string(": ") + strerror(errno)) {}

  explicit runtime_error_with_errno(const string& msg, int e) throw()
    : runtime_error(msg + string(": ") + strerror(e)) {}
};

// Utility functions
void assert_argument(char** argv, int argc, int index);

string itos(unsigned int value);

string mtos(mode_t mode);

string trim_filename(const string& filename);

bool file_exists(const string& filename);

bool file_empty(const string& filename);

bool file_equal(const string&  file1,
                const string&  file2);

bool permissions_equal(const string&  file1,
                       const string&  file2);

void file_remove(const string&  basedir,
                 const string&  filename);

#endif /* PKGUTIL_H */

// vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
// End of file.
