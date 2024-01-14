//! \file  pkgutil.cpp
//! \brief pkgutil helpers implementation.
//!        See COPYING and COPYRIGHT files for corresponding information.

#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <csignal>

#include <ext/stdio_filebuf.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
/* libarchive */
#include <archive.h>
#include <archive_entry.h>

#include "pkgutil.h"

#define INIT_ARCHIVE(ar)                    \
  archive_read_support_filter_gzip((ar));   \
  archive_read_support_filter_bzip2((ar));  \
  archive_read_support_filter_xz((ar));     \
  archive_read_support_filter_lzip((ar));   \
  archive_read_support_filter_zstd((ar));   \
  archive_read_support_format_tar((ar))

#define DEFAULT_BYTES_PER_BLOCK (20 * 512)

using __gnu_cxx::stdio_filebuf;

pkgutil::pkgutil(const string& name)
  : utilname(name)
{
  /*
   * Ignore signals.
   */
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigaction(SIGHUP,  &sa, 0);
  sigaction(SIGINT,  &sa, 0);
  sigaction(SIGQUIT, &sa, 0);
  sigaction(SIGTERM, &sa, 0);
}

void
pkgutil::db_open(const string& path)
{
  /*
   * Read database.
   */
  root = trim_filename(path + "/");
  const string filename = root + PKG_DB;

  int fd = open(filename.c_str(), O_RDONLY);
  if (fd == -1)
    throw runtime_error_with_errno("could not open " + filename);

  stdio_filebuf<char> filebuf(fd, ios::in, getpagesize());
  istream in(&filebuf);
  if (!in)
    throw runtime_error_with_errno("could not read " + filename);

  while (!in.eof())
  {
    /*
     * Read record.
     */
    string    name;
    pkginfo_t info;

    getline(in, name);
    getline(in, info.version);

    for (;;)
    {
      string file;
      getline(in, file);

      if (file.empty())
        break; /* End of record. */

      info.files.insert(info.files.end(), file);
    }
    if (!info.files.empty())
      packages[name] = info;
  }

#ifndef NDEBUG
  cerr << packages.size() << " packages found in database" << endl;
#endif
}

void
pkgutil::db_commit()
{
  const string dbfilename     = root + PKG_DB;
  const string dbfilename_new = dbfilename + ".incomplete_transaction";
  const string dbfilename_bak = dbfilename + ".backup";

  /*
   * Remove failed transaction (if it exists).
   */
  if (unlink(dbfilename_new.c_str()) == -1 && errno != ENOENT)
    throw runtime_error_with_errno("could not remove " +
                                    dbfilename_new);

  /*
   * Write new database.
   */
  int fd_new = creat(dbfilename_new.c_str(), 0444);
  if (fd_new == -1)
    throw runtime_error_with_errno("could not create " +
                                    dbfilename_new);

  stdio_filebuf<char> filebuf_new(fd_new, ios::out, getpagesize());
  ostream db_new(&filebuf_new);

  for (packages_t::const_iterator
        i = packages.begin(); i != packages.end(); ++i)
  {
    if (!i->second.files.empty())
    {
      db_new << i->first << "\n";
      db_new << i->second.version << "\n";
      copy(i->second.files.begin(), i->second.files.end(),
           ostream_iterator<string>(db_new, "\n"));
      db_new << "\n";
    }
  }

  db_new.flush();

  /*
   * Make sure the new database was successfully written.
   */
  if (!db_new)
    throw runtime_error("could not write " + dbfilename_new);

  /*
   * Synchronize file to disk.
   */
  if (fsync(fd_new) == -1)
    throw runtime_error_with_errno("could not synchronize " +
                                    dbfilename_new);

  /*
   * Relink database backup.
   */
  if (unlink(dbfilename_bak.c_str()) == -1 && errno != ENOENT)
    throw runtime_error_with_errno("could not remove " +
                                    dbfilename_bak);

  if (link(dbfilename.c_str(), dbfilename_bak.c_str()) == -1)
    throw runtime_error_with_errno("could not create " +
                                    dbfilename_bak);

  /*
   * Move new database into place.
   */
  if (rename(dbfilename_new.c_str(), dbfilename.c_str()) == -1)
    throw runtime_error_with_errno("could not rename " +
                                dbfilename_new + " to " + dbfilename);

#ifndef NDEBUG
  cerr << packages.size() << " packages written to database" << endl;
#endif
}

void
pkgutil::db_add_pkg(const string& name, const pkginfo_t& info)
{
  packages[name] = info;
}

bool
pkgutil::db_find_pkg(const string& name)
{
  return (packages.find(name) != packages.end());
}

void
pkgutil::db_rm_pkg(const string& name)
{
  set<string> files = packages[name].files;
  packages.erase(name);

#ifndef NDEBUG
  cerr << "Removing package phase 1 (all files in package):" << endl;
  copy(files.begin(), files.end(),
       ostream_iterator<string>(cerr, "\n"));
  cerr << endl;
#endif

  /*
   * Don't delete files that still have references.
   */
  for (packages_t::const_iterator
        i = packages.begin(); i != packages.end(); ++i)
  {
    for (set<string>::const_iterator
          j = i->second.files.begin(); j != i->second.files.end(); ++j)
    {
      files.erase(*j);
    }
  }

#ifndef NDEBUG
  cerr << "Removing package phase 2 "
       << "(files that still have references excluded):"
       << endl;
  copy(files.begin(), files.end(),
       ostream_iterator<string>(cerr, "\n"));
  cerr << endl;
#endif

  /*
   * Delete the files.
   */
  for (set<string>::const_reverse_iterator
        i = files.rbegin(); i != files.rend(); ++i)
  {
    const string filename = root + *i;
    if (file_exists(filename) && remove(filename.c_str()) == -1)
    {
      const char* msg = strerror(errno);
      cerr << utilname << ": could not remove " << filename << ": "
           << msg << endl;
    }
  }
}

void
pkgutil::db_rm_pkg(const string& name, const set<string>& keep_list)
{
  set<string> files = packages[name].files;
  packages.erase(name);

#ifndef NDEBUG
  cerr << "Removing package phase 1 (all files in package):" << endl;
  copy(files.begin(), files.end(),
       ostream_iterator<string>(cerr, "\n"));
  cerr << endl;
#endif

  /*
   * Don't delete files found in the keep list.
   */
  for (set<string>::const_iterator
        i = keep_list.begin(); i != keep_list.end(); ++i)
  {
    files.erase(*i);
  }

#ifndef NDEBUG
  cerr << "Removing package phase 2 "
       << "(files that is in the keep list excluded):"
       << endl;
  copy(files.begin(), files.end(),
       ostream_iterator<string>(cerr, "\n"));
  cerr << endl;
#endif

  /*
   * Don't delete files that still have references.
   */
  for (packages_t::const_iterator
        i = packages.begin(); i != packages.end(); ++i)
  {
    for (set<string>::const_iterator
          j = i->second.files.begin(); j != i->second.files.end(); ++j)
    {
      files.erase(*j);
    }
  }

#ifndef NDEBUG
  cerr << "Removing package phase 3 "
       << "(files that still have references excluded):"
       << endl;
  copy(files.begin(), files.end(),
       ostream_iterator<string>(cerr, "\n"));
  cerr << endl;
#endif

  /*
   * Delete the files.
   */
  for (set<string>::const_reverse_iterator
        i = files.rbegin(); i != files.rend(); ++i)
  {
    const string filename = root + *i;
    if (file_exists(filename) && remove(filename.c_str()) == -1)
    {
      if (errno == ENOTEMPTY)
        continue;

      const char* msg = strerror(errno);
      cerr << utilname << ": could not remove " << filename << ": "
           << msg << endl;
    }
  }
}

void
pkgutil::db_rm_files(set<string> files, const set<string>& keep_list)
{
  /*
   * Remove all references.
   */
  for (packages_t::iterator
        i = packages.begin(); i != packages.end(); ++i)
  {
    for (set<string>::const_iterator
          j = files.begin(); j != files.end(); ++j)
    {
      i->second.files.erase(*j);
    }
  }

#ifndef NDEBUG
  cerr << "Removing files:" << endl;
  copy(files.begin(), files.end(),
       ostream_iterator<string>(cerr, "\n"));
  cerr << endl;
#endif

  /*
   * Don't delete files found in the keep list.
   */
  for (set<string>::const_iterator
        i = keep_list.begin(); i != keep_list.end(); ++i)
  {
    files.erase(*i);
  }

  /*
   * Delete the files.
   */
  for (set<string>::const_reverse_iterator
        i = files.rbegin(); i != files.rend(); ++i)
  {
    const string filename = root + *i;
    if (file_exists(filename) && remove(filename.c_str()) == -1)
    {
      if (errno == ENOTEMPTY)
        continue;

      const char* msg = strerror(errno);
      cerr << utilname << ": could not remove " << filename << ": "
           << msg << endl;
    }
  }
}

set<string>
pkgutil::db_find_conflicts(const string& name, const pkginfo_t&  info)
{
  set<string> files;

  /*
   * Find conflicting files in database.
   */
  for (packages_t::const_iterator
        i = packages.begin(); i != packages.end(); ++i)
  {
    if (i->first != name)
    {
      set_intersection(info.files.begin(), info.files.end(),
           i->second.files.begin(), i->second.files.end(),
           inserter(files, files.end()));
    }
  }

#ifndef NDEBUG
  cerr << "Conflicts phase 1 (conflicts in database):" << endl;
  copy(files.begin(), files.end(),
       ostream_iterator<string>(cerr, "\n"));
  cerr << endl;
#endif

  /*
   * Find conflicting files in filesystem.
   */
  for (set<string>::iterator
        i = info.files.begin(); i != info.files.end(); ++i)
  {
    const string filename = root + *i;

    if (file_exists(filename) && files.find(*i) == files.end())
      files.insert(files.end(), *i);
  }

#ifndef NDEBUG
  cerr << "Conflicts phase 2 (conflicts in filesystem added):" << endl;
  copy(files.begin(), files.end(),
       ostream_iterator<string>(cerr, "\n"));
  cerr << endl;
#endif

  /*
   * Exclude directories.
   */
  set<string> tmp = files;
  for (set<string>::const_iterator
        i = tmp.begin(); i != tmp.end(); ++i)
  {
    if ((*i)[i->length() - 1] == '/')
      files.erase(*i);
  }

#ifndef NDEBUG
  cerr << "Conflicts phase 3 (directories excluded):" << endl;
  copy(files.begin(), files.end(),
       ostream_iterator<string>(cerr, "\n"));
  cerr << endl;
#endif

  /*
   * If this is an upgrade, remove files already owned by this
   * package.
   */
  if (packages.find(name) != packages.end())
  {
    for (set<string>::const_iterator
          i  = packages[name].files.begin();
          i != packages[name].files.end();
          ++i)
    {
      files.erase(*i);
    }

#ifndef NDEBUG
    cerr << "Conflicts phase 4 "
         << "(files already owned by this package excluded):"
         << endl;
    copy(files.begin(), files.end(),
         ostream_iterator<string>(cerr, "\n"));
    cerr << endl;
#endif
  }

  return files;
}

pair<string, pkgutil::pkginfo_t>
pkgutil::pkg_open(const string& filename)
  const
{
  pair<string, pkginfo_t> result;
  unsigned int i;
  struct archive* archive;
  struct archive_entry* entry;

  /*
   * Extract name and version from filename.
   */
  string basename(filename, filename.rfind('/') + 1);
  string name(basename, 0, basename.find(VERSION_DELIM));
  string version(basename, 0, basename.rfind(PKG_EXT));
  version.erase(0,
      version.find(VERSION_DELIM) == string::npos
        ? string::npos
        : version.find(VERSION_DELIM) + 1);

  if (name.empty() || version.empty())
  {
    throw runtime_error("could not determine name and/or version of " +
                         basename + ": Invalid package name");
  }

  result.first = name;
  result.second.version = version;

  archive = archive_read_new();
  INIT_ARCHIVE(archive);

  if (archive_read_open_filename(archive,
                                 filename.c_str(),
                                 DEFAULT_BYTES_PER_BLOCK)
      != ARCHIVE_OK)
  {
    throw runtime_error_with_errno("could not open " + filename,
                                   archive_errno(archive));
  }

  for (i = 0;
        archive_read_next_header(archive, &entry) == ARCHIVE_OK;
        ++i)
  {
    result.second.files.insert(result.second.files.end(),
                               archive_entry_pathname(entry));

    mode_t mode = archive_entry_mode(entry);

    if (   S_ISREG(mode)
        && archive_read_data_skip(archive) != ARCHIVE_OK)
    {
      throw runtime_error_with_errno("could not read " + filename,
          archive_errno(archive));
    }
  }

  if (i == 0)
  {
    if (archive_errno(archive) == 0)
      throw runtime_error("empty package");
    else
      throw runtime_error("could not read " + filename);
  }

  archive_read_free(archive);

  return result;
}

void
pkgutil::pkg_install(const string& filename,
                     const set<string>& keep_list,
                     const set<string>& non_install_list,
                     bool upgrade)
  const
{
  struct archive*        archive;
  struct archive_entry*  entry;
  unsigned int           i;
  char                   buf[PATH_MAX];
  string                 absroot;

  archive = archive_read_new();
  INIT_ARCHIVE(archive);

  if (archive_read_open_filename(archive,
                                 filename.c_str(),
                                 DEFAULT_BYTES_PER_BLOCK)
      != ARCHIVE_OK)
  {
    throw runtime_error_with_errno("could not open " + filename,
        archive_errno(archive));
  }

  chdir(root.c_str());
  absroot = getcwd(buf, sizeof(buf));

  for (i = 0;
        archive_read_next_header(archive, &entry) == ARCHIVE_OK;
        ++i)
  {
    string archive_filename = archive_entry_pathname(entry);

    string reject_dir =
      trim_filename(absroot + string("/") + string(PKG_REJECTED));

    string original_filename =
      trim_filename(absroot + string("/") + archive_filename);

    string real_filename = original_filename;

    /*
     * Check if file is filtered out via INSTALL.
     */
    if (non_install_list.find(archive_filename)
        != non_install_list.end())
    {
      mode_t mode;

      cout << utilname << ": ignoring " << archive_filename << endl;

      mode = archive_entry_mode(entry);

      if (S_ISREG(mode))
        archive_read_data_skip(archive);

      continue;
    }

    /*
     * Check if file should be rejected.
     */
    if (   file_exists(real_filename)
        && keep_list.find(archive_filename) != keep_list.end())
    {
      real_filename =
        trim_filename(reject_dir + string("/") + archive_filename);
    }

    archive_entry_set_pathname(entry,
        const_cast<char*>(real_filename.c_str()));

    /*
     * Extract file.
     */
    auto flags =
        ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM
      | ARCHIVE_EXTRACT_TIME  | ARCHIVE_EXTRACT_UNLINK;

    if (archive_read_extract(archive, entry, flags) != ARCHIVE_OK)
    {
      /* If a file fails to install we just print an error message and
       * continue trying to install the rest of the package, unless
       * this is not an upgrade. */
      const char* msg = archive_error_string(archive);
      cerr << utilname << ": could not install " +
        archive_filename << ": " << msg << endl;

      if (!upgrade)
      {
        throw runtime_error("extract error: " + archive_filename +
                            ": " + msg);
      }
      continue;
    }

    /*
     * Check rejected file.
     */
    if (real_filename != original_filename)
    {
      bool remove_file = false;
      mode_t mode = archive_entry_mode(entry);

      /* directory */
      if (S_ISDIR(mode))
      {
        remove_file = permissions_equal(real_filename,
                                        original_filename);
      }
      /* other files */
      else
      {
        remove_file =
             permissions_equal(real_filename, original_filename)
          && (   file_empty(real_filename)
              || file_equal(real_filename, original_filename));
      }

      /* remove rejected file or signal about its existence */
      if (remove_file)
        file_remove(reject_dir, real_filename);
      else
        cout << utilname << ": rejecting " << archive_filename
             << ", keeping existing version" << endl;
    }
  }

  if (i == 0)
  {
    if (archive_errno(archive) == 0)
      throw runtime_error("empty package");
    else
      throw runtime_error("could not read " + filename);
  }

  archive_read_free(archive);
}

void
pkgutil::ldconfig()
  const
{
  /*
   * Only execute ldconfig if /etc/ld.so.conf exists.
   */
  if (file_exists(root + LDCONFIG_CONF))
  {
    pid_t pid = fork();

    if (pid == -1)
      throw runtime_error_with_errno("fork() failed");

    if (pid == 0)
    {
      execl(LDCONFIG, LDCONFIG, "-r", root.c_str(), (char *) 0);
      const char* msg = strerror(errno);
      cerr << utilname << ": could not execute " << LDCONFIG << ": "
           << msg << endl;
      exit(EXIT_FAILURE);
    }
    else
    {
      if (waitpid(pid, 0, 0) == -1)
        throw runtime_error_with_errno("waitpid() failed");
    }
  }
}

void
pkgutil::pkg_footprint(const string& filename)
  const
{
  size_t i;
  struct archive* archive;
  struct archive_entry* entry;

  struct file {
    string path;
    string soft;
    string hard;
    off_t  size;
    dev_t  rdev;
    uid_t  uid;
    gid_t  gid;
    mode_t mode;
    bool operator < (const struct file& other)
    {
      return (path < other.path);
    }
    bool operator < (const string& other)
    {
      return (path < other);
    }
  };

  vector<struct file> files;

  /*
   * We first do a run over the archive and remember the modes of
   * regular files.  In the second run, we print the footprint - using
   * the stored modes for hardlinks.
   *
   * FIXME the code duplication here is butt ugly.
   */
  archive = archive_read_new();
  INIT_ARCHIVE(archive);

  if (archive_read_open_filename(archive,
                                 filename.c_str(),
                                 DEFAULT_BYTES_PER_BLOCK)
      != ARCHIVE_OK)
  {
    throw runtime_error_with_errno("could not open " + filename,
        archive_errno(archive));
  }

  for (i = 0;
        archive_read_next_header(archive, &entry) == ARCHIVE_OK;
        ++i)
  {

    struct file file;
    const char* s;

    if ((s = archive_entry_pathname(entry)))
      file.path = s;

    if ((s = archive_entry_symlink(entry)))
      file.soft = s;

    if ((s = archive_entry_hardlink(entry)))
      file.hard = s;

    file.size = archive_entry_size(entry);
    file.rdev = archive_entry_rdev(entry);
    file.uid  = archive_entry_uid(entry);
    file.gid  = archive_entry_gid(entry);
    file.mode = archive_entry_mode(entry);

    files.push_back(file);

    if (S_ISREG(file.mode) && archive_read_data_skip(archive))
    {
      throw runtime_error_with_errno("could not read " + filename,
                                      archive_errno(archive));
    }
  }

  if (i == 0)
  {
    if (archive_errno(archive) == 0)
      throw runtime_error("empty package");
    else
      throw runtime_error("could not read " + filename);
  }

  archive_read_free(archive);

  sort(files.begin(), files.end());

  for (i = 0; i < files.size(); ++i)
  {
    struct file& file = files[i];

    /*
     * Access permissions.
     */
    if (S_ISLNK(file.mode))
    {
      /* Access permissions on symlinks differ among filesystems,
       * e.g. XFS and ext2 have different.
       *
       * To avoid getting different footprints we always use
       * "lrwxrwxrwx".
       */
      cout << "lrwxrwxrwx";
    }
    else
    {
      if (file.hard.length())
      {
        auto it = lower_bound(files.begin(), files.end(), file.hard);
        cout << mtos(it->mode);
      }
      else
        cout << mtos(file.mode);
    }

    cout << '\t';

    /*
     * User.
     */
    struct passwd* pw = getpwuid(file.uid);
    if (pw)
      cout << pw->pw_name;
    else
      cout << file.uid;

    cout << '/';

    /*
     * Group.
     */
    struct group* gr = getgrgid(file.gid);
    if (gr)
      cout << gr->gr_name;
    else
      cout << file.gid;

    /*
     * Filename.
     */
    cout << '\t' << file.path;

    /*
     * Special cases.
     */
    if (S_ISLNK(file.mode))
    {
      /* Symlink. */
      cout << " -> " << file.soft;
    }
    else if (S_ISCHR(file.mode) || S_ISBLK(file.mode))
    {
      /* Device. */
      cout << " (" << major(file.rdev) << ", " << minor(file.rdev) << ")";
    }
    else if (S_ISREG(file.mode) && file.size == 0)
    {
      /* Empty regular file. */
      cout << " (EMPTY)";
    }

    cout << '\n';
  }
}

void
pkgutil::print_version()
  const
{
  cout << utilname << " (pkgutils) " << VERSION << endl;
}

db_lock::db_lock(const string& root, bool exclusive)
  : dir(0)
{
  const string dirname = trim_filename(root + string("/") + PKG_DIR);

  if (!(dir = opendir(dirname.c_str())))
    throw runtime_error_with_errno("could not read directory " +
                                    dirname);

  if (flock(dirfd(dir),
        (exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) == -1)
  {
    if (errno == EWOULDBLOCK)
      throw runtime_error(
          "package database is currently locked by another process");
    else
      throw runtime_error_with_errno("could not lock directory " +
                                      dirname);
  }
}

db_lock::~db_lock()
{
  if (dir)
  {
    flock(dirfd(dir), LOCK_UN);
    closedir(dir);
  }
}

string
itos(unsigned int value)
{
  static char buf[20];
  sprintf(buf, "%u", value);
  return buf;
}

string
mtos(mode_t mode)
{
  string s;

  /*
   * File type.
   */
  switch (mode & S_IFMT)
  {
     case S_IFREG:  s += '-'; break; /* Regular           */
     case S_IFDIR:  s += 'd'; break; /* Directory         */
     case S_IFLNK:  s += 'l'; break; /* Symbolic link     */
     case S_IFCHR:  s += 'c'; break; /* Character special */
     case S_IFBLK:  s += 'b'; break; /* Block special     */
     case S_IFSOCK: s += 's'; break; /* Socket            */
     case S_IFIFO:  s += 'p'; break; /* FIFO              */
     default:       s += '?'; break; /* Unknown           */
  }

  /*
   * User permissions.
   */
  s += (mode & S_IRUSR) ? 'r' : '-';
  s += (mode & S_IWUSR) ? 'w' : '-';
  switch (mode & (S_IXUSR | S_ISUID))
  {
    case S_IXUSR:           s += 'x'; break;
    case S_ISUID:           s += 'S'; break;
    case S_IXUSR | S_ISUID: s += 's'; break;
    default:                s += '-'; break;
  }

  /*
   * Group permissions.
   */
  s += (mode & S_IRGRP) ? 'r' : '-';
  s += (mode & S_IWGRP) ? 'w' : '-';
  switch (mode & (S_IXGRP | S_ISGID))
  {
    case S_IXGRP:           s += 'x'; break;
    case S_ISGID:           s += 'S'; break;
    case S_IXGRP | S_ISGID: s += 's'; break;
    default:                s += '-'; break;
  }

  /*
   * Other permissions.
   */
  s += (mode & S_IROTH) ? 'r' : '-';
  s += (mode & S_IWOTH) ? 'w' : '-';
  switch (mode & (S_IXOTH | S_ISVTX))
  {
    case S_IXOTH:           s += 'x'; break;
    case S_ISVTX:           s += 'T'; break;
    case S_IXOTH | S_ISVTX: s += 't'; break;
    default:                s += '-'; break;
  }

  return s;
}

string
trim_filename(const string& filename)
{
  string search("//");
  string result = filename;

  for (string::size_type
         pos  = result.find(search);
         pos != string::npos;
         pos  = result.find(search))
  {
    result.replace(pos, search.size(), "/");
  }

  return result;
}

bool
file_exists(const string& filename)
{
  struct stat buf;
  return !lstat(filename.c_str(), &buf);
}

bool
file_empty(const string& filename)
{
  struct stat buf;

  if (lstat(filename.c_str(), &buf) == -1)
    return false;

  return (S_ISREG(buf.st_mode) && buf.st_size == 0);
}

bool
file_equal(const string& file1, const string& file2)
{
  struct stat buf1, buf2;

  if (lstat(file1.c_str(), &buf1) == -1)
    return false;

  if (lstat(file2.c_str(), &buf2) == -1)
    return false;

  /*
   * Regular files.
   */
  if (S_ISREG(buf1.st_mode) && S_ISREG(buf2.st_mode))
  {
    ifstream f1(file1.c_str());
    ifstream f2(file2.c_str());

    if (!f1 || !f2)
      return false;

    while (!f1.eof())
    {
      char buffer1[4096];
      char buffer2[4096];

      f1.read(buffer1, 4096);
      f2.read(buffer2, 4096);

      if (   f1.gcount() != f2.gcount()
          || memcmp(buffer1, buffer2, f1.gcount())
          || f1.eof() != f2.eof())
        return false;
    }

    return true;
  }
  /*
   * Symlinks.
   */
  else if (S_ISLNK(buf1.st_mode) && S_ISLNK(buf2.st_mode))
  {
    char symlink1[MAXPATHLEN];
    char symlink2[MAXPATHLEN];

    memset(symlink1, 0, MAXPATHLEN);
    memset(symlink2, 0, MAXPATHLEN);

    if (readlink(file1.c_str(), symlink1, MAXPATHLEN - 1) == -1)
      return false;

    if (readlink(file2.c_str(), symlink2, MAXPATHLEN - 1) == -1)
      return false;

    return !strncmp(symlink1, symlink2, MAXPATHLEN);
  }
  /*
   * Character devices.
   */
  else if (S_ISCHR(buf1.st_mode) && S_ISCHR(buf2.st_mode))
  {
    return buf1.st_dev == buf2.st_dev;
  }
  /*
   * Block devices.
   */
  else if (S_ISBLK(buf1.st_mode) && S_ISBLK(buf2.st_mode))
  {
    return buf1.st_dev == buf2.st_dev;
  }

  return false;
}

bool
permissions_equal(const string& file1, const string& file2)
{
  struct stat buf1;
  struct stat buf2;

  if (lstat(file1.c_str(), &buf1) == -1)
    return false;

  if (lstat(file2.c_str(), &buf2) == -1)
    return false;

  return  (buf1.st_mode == buf2.st_mode)
       && (buf1.st_uid  == buf2.st_uid)
       && (buf1.st_gid  == buf2.st_gid);
}

void
file_remove(const string& basedir, const string& filename)
{
  if (filename != basedir && !remove(filename.c_str()))
  {
    char* path = strdup(filename.c_str());
    file_remove(basedir, dirname(path));
    free(path);
  }
}

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.
