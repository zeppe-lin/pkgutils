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


//! \def INIT_ARCHIVE(ar)
//! \brief Initializes a libarchive read object for common
//!        archive formats.
//! \param ar The archive read object (`struct archive*`) to
//!        initialize.
//!
//! \details
//! This macro simplifies the initialization of a libarchive read
//! object by enabling support for various compression filters and
//! the tar format.  It enables gzip, bzip2, xz, lzip, and zstd
//! decompression filters, as well as tar archive format support.
#define INIT_ARCHIVE(ar)                    \
  archive_read_support_filter_gzip((ar));   \
  archive_read_support_filter_bzip2((ar));  \
  archive_read_support_filter_xz((ar));     \
  archive_read_support_filter_lzip((ar));   \
  archive_read_support_filter_zstd((ar));   \
  archive_read_support_format_tar((ar))


//! \def DEFAULT_BYTES_PER_BLOCK
//! \brief Defines the default block size for archive operations.
//!
//! \details
//! This macro defines the default block size, set to 20 * 512
//! bytes (10240 bytes), used for reading data blocks from archives.
#define DEFAULT_BYTES_PER_BLOCK (20 * 512)

using __gnu_cxx::stdio_filebuf;

/**
 * \brief Constructor for the pkgutil class.
 * \param name The name of the utility.
 *
 * \details
 * Initializes the `pkgutil` object with the given utility name.
 * This constructor also sets up signal handlers to ignore SIGHUP,
 * SIGINT, SIGQUIT, and SIGTERM signals. This is likely to prevent
 * the utility from being interrupted by these signals during
 * critical operations.
 */
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
/**
 * \brief Opens and reads the package database from disk.
 * \param path The path to the directory containing the package
 *        database.
 * \throws runtime_error_with_errno If the database file cannot be
 *        opened or read.
 *
 * \details
 * This function opens the package database file (PKG_DB) located
 * within the specified `path` directory. It reads each record from
 * the database, which consists of a package name, package version,
 * and a list of files belonging to the package. This data is
 * loaded into the `packages` map, where the key is the package
 * name and the value is a `pkginfo_t` structure.
 *
 * The database file is expected to have the following format:
 * - Package Name (line)
 * - Package Version (line)
 * - File Path 1 (line)
 * - File Path 2 (line)
 * - ...
 * - File Path N (line)
 * - Empty Line (Record Separator)
 * - ... (Next Package Record)
 *
 * Redundant and trailing slashes in the provided path are trimmed
 * using `trim_filename`.
 */
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

/**
 * \brief Commits changes to the package database, writing it to
 *        disk atomically.
 * \throws runtime_error_with_errno If any file operation during
 *        commit fails.
 * \throws runtime_error If writing to the new database file fails.
 *
 * \details
 * This function commits changes made to the in-memory `packages`
 * database by writing the updated database to disk. It implements
 * an atomic commit strategy to prevent database corruption in case
 * of errors or interruptions during the write process.
 *
 * The atomic commit process involves the following steps:
 * 1. Removes any existing incomplete transaction file
 *    (`.incomplete_transaction`).
 * 2. Creates a new database file with a temporary name
 *    (`.incomplete_transaction`) and writes the current in-memory
 *    `packages` database to this new file.
 * 3. Flushes and synchronizes the new database file to disk to
 *    ensure data persistence.
 * 4. Backs up the old database file by renaming it to `.backup`.
 * 5. Renames the new database file (from `.incomplete_transaction`)
 *    to the final database filename, effectively replacing the old
 *    database with the new one.
 *
 * This process ensures that if any error occurs during the commit,
 * the old database remains intact (backed up as `.backup`), and no
 * corrupted or partially written database is left in place.
 */
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

/**
 * \brief Adds a package to the in-memory database.
 * \param name The name of the package.
 * \param info The package information (`pkginfo_t`) to add.
 *
 * \details
 * This function adds a package and its associated information to the
 * in-memory `packages` map. If a package with the same name already
 * exists, it will be overwritten.
 */
void
pkgutil::db_add_pkg(const string& name, const pkginfo_t& info)
{
  packages[name] = info;
}

/**
 * \brief Checks if a package with the given name exists in the
 *        database.
 * \param name The name of the package to search for.
 * \return True if the package exists in the database, false otherwise.
 */
bool
pkgutil::db_find_pkg(const string& name)
{
  return (packages.find(name) != packages.end());
}

/**
 * \brief Removes a package and its files from the database and
 *        attempts to delete the files from the filesystem.
 * \param name The name of the package to remove.
 *
 * \details
 * This function removes a package entry from the `packages`
 * database and then attempts to delete the files associated with
 * that package from the filesystem. It performs a two-phase
 * removal process to ensure files are only deleted if they are no
 * longer referenced by other packages in the database.
 *
 * Phase 1: Collects all files belonging to the package being removed.
 * Phase 2: Iterates through all remaining packages in the database
 *          and removes any files from the collected file list that
 *          are still referenced by other packages. This ensures
 *          shared files are not deleted prematurely.
 * Phase 3: Deletes the remaining files in the collected list from
 *          the filesystem. File deletion is attempted using
 *          `remove()`. If deletion fails due to errors other than
 *          `ENOTEMPTY`, an error message is printed to stderr.
 *
 * \note File deletion is attempted even if `remove()` fails with
 *      `ENOTEMPTY` (directory not empty), which might indicate a
 *      shared directory or files placed in the package directory
 *      after installation.
 */
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

/**
 * \brief Removes a package from the database, keeping a specified
 *        list of files.
 * \param name The name of the package to remove.
 * \param keep_list A set of file paths to keep (not remove from
 *                 database and filesystem).
 *
 * \details
 * This function removes a package from the `packages` database, but
 * preserves files listed in the `keep_list`. Similar to
 * `db_rm_pkg(const string& name)`, it attempts to delete
 * associated files from the filesystem, but excludes files present
 * in the `keep_list` and files still referenced by other packages.
 *
 * The removal process consists of three phases:
 * Phase 1: Collects all files belonging to the package.
 * Phase 2: Excludes files present in the `keep_list` from the
 *          collected file list.
 * Phase 3: Excludes files from the remaining list that are still
 *          referenced by other packages in the database.
 * Phase 4: Deletes the remaining files in the collected list from
 *          the filesystem. File deletion is attempted using
 *          `remove()`. If deletion fails due to errors other than
 *          `ENOTEMPTY`, an error message is printed to stderr.
 *
 * \note Files in `keep_list` are protected from deletion in both
 *      database and filesystem removal.
 * \note File deletion is attempted even if `remove()` fails with
 *      `ENOTEMPTY`, similar to `db_rm_pkg(const string& name)`.
 */
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

/**
 * \brief Removes specific files from the database and attempts to
 *        delete them from the filesystem.
 * \param files A set of file paths to remove from the database and
 *             filesystem.
 * \param keep_list A set of file paths to keep (not remove from
 *                 database and filesystem).
 *
 * \details
 * This function removes specified files from the database and
 * attempts to delete them from the filesystem. Files present in the
 * `keep_list` are excluded from deletion in both database and
 * filesystem.
 *
 * The removal process includes:
 * Phase 1: Removes references to the files being removed from all
 *          packages in the database.
 * Phase 2: Excludes files present in the `keep_list` from the set
 *          of files to be removed.
 * Phase 3: Deletes the remaining files from the filesystem using
 *          `remove()`. File deletion is attempted using `remove()`.
 *          If deletion fails due to errors other than `ENOTEMPTY`,
 *          an error message is printed to stderr.
 *
 * \note Files in `keep_list` are protected from deletion.
 * \note File deletion is attempted even if `remove()` fails with
 *      `ENOTEMPTY`, similar to package removal functions.
 */
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

/**
 * \brief Finds file conflicts for a package installation.
 * \param name The name of the package being installed.
 * \param info Package information (`pkginfo_t`) of the package to
 *             be installed.
 * \return A set of file paths that conflict with the package being
 *             installed.
 *
 * \details
 * This function identifies file conflicts that would arise if the
 * package specified by `name` and `info` were to be installed. It
 * checks for conflicts in both the existing package database and
 * the filesystem.
 *
 * The conflict detection process is performed in four phases:
 * Phase 1: Finds files in the database that are also present in the
 *          package being installed and belong to a different package.
 * Phase 2: Finds files in the filesystem (under the installation
 *          root) that are also present in the package being
 *          installed but are not already identified as conflicts in
 *          the database (Phase 1).
 * Phase 3: Excludes directories from the set of conflicting files.
 *          Conflicts are only reported for regular files, symlinks,
 *          etc., not directories.
 * Phase 4: If the package being installed is an upgrade of an
 *          existing package (checked by looking up the package name
 *          in the database), files that are already owned by the
 *          package being upgraded are excluded from the conflict
 *          set. This is because upgrading a package over itself
 *          should not report conflicts for files it already owns.
 *
 * \return A set of strings representing the paths of conflicting
 *          files.
 */
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

/**
 * \brief Opens a package file and extracts package information
 *        (name, version, file list).
 * \param filename The path to the package file.
 * \return A pair containing the package name (string) and package
 *          information (`pkginfo_t`).
 * \throws runtime_error_with_errno If the package file cannot be
 *        opened or read by libarchive.
 * \throws runtime_error If the package file is empty or if package
 *        name/version cannot be determined.
 *
 * \details
 * This function opens a package file (assumed to be a tar archive
 * compressed with gzip, bzip2, xz, lzip or zstd) using libarchive
 * and extracts package metadata and the list of files contained
 * within the archive.
 *
 * The package name and version are extracted from the filename
 * itself, based on predefined delimiters (VERSION_DELIM, PKG_EXT).
 * The filename is expected to follow a naming convention like
 * `package_name-version.PKG_EXT`.
 *
 * The function iterates through the archive entries and collects
 * the pathname of each entry as a file belonging to the package.
 * It skips reading the actual data content of regular files during
 * this process to improve efficiency.
 *
 * If the archive is empty, or if the package name or version
 * cannot be parsed from the filename, or if libarchive encounters
 * an error during opening or reading the archive, a
 * `runtime_error` or `runtime_error_with_errno` exception is
 * thrown.
 */
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

/**
 * \brief Installs a package from a package file.
 * \param filename The path to the package file.
 * \param keep_list A set of file paths to keep if they already
 *                 exist on the system.
 * \param non_install_list A set of file paths within the package
 *                         that should not be installed.
 * \param upgrade A boolean flag indicating if this is an upgrade
 *                 operation.
 * \throws runtime_error_with_errno If the package file cannot be
 *        opened or read by libarchive.
 * \throws runtime_error If file extraction fails for a non-upgrade
 *        installation.
 *
 * \details
 * This function installs a package from the specified package file.
 * It uses libarchive to read the package archive and extract its
 * contents to the filesystem under the root directory defined in
 * `db_open()`.
 *
 * The installation process includes the following steps:
 * 1. Opens the package file using libarchive.
 * 2. Sets the current working directory to the installation root.
 * 3. Iterates through each entry in the archive:
 *    - Checks if the file is in the `non_install_list`; if so, it
 *       is ignored.
 *    - Checks if the file path is in the `keep_list`. If it is and
 *       the file already exists on the system, the file is rejected
 *       (moved to a rejected directory `PKG_REJECTED`) instead of
 *       being installed, and existing files are preserved.
 *    - Extracts the file from the archive to the filesystem using
 *       `archive_read_extract()`, preserving owner, permissions,
 *       and timestamps.
 *    - If file extraction fails and it's not an upgrade
 *       operation, a `runtime_error` is thrown.
 *    - For rejected files (files in `keep_list` that already
 *       existed):
 *      - Compares permissions and content (for non-directories) of
 *         the rejected file with the original file. If they are
 *         considered equal (permissions and content match), the
 *         rejected file is removed. Otherwise, a message is
 *         printed indicating that the file was rejected, and the
 *         existing version is kept.
 *
 * \note If file extraction fails during an upgrade, the
 *      installation continues with the rest of the package.
 * \note The function uses `ARCHIVE_EXTRACT_*` flags to control
 *      extraction behavior (owner, permissions, etc.).
 * \note Rejection logic involves moving files to a `PKG_REJECTED`
 *      subdirectory to avoid overwriting existing files in
 *      `keep_list`.
 */
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
      | ARCHIVE_EXTRACT_TIME  | ARCHIVE_EXTRACT_UNLINK
#ifdef ENABLE_EXTRACT_ACL
      | ARCHIVE_EXTRACT_ACL
#endif
#ifdef ENABLE_EXTRACT_XATTR
      | ARCHIVE_EXTRACT_XATTR
#endif
      ;

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

/**
 * \brief Executes the ldconfig utility to update shared library
 *        cache.
 *
 * \details
 * This function executes the `ldconfig` utility, typically located
 * at `/sbin/ldconfig` or `/usr/sbin/ldconfig`, to update the
 * shared library cache. It only executes `ldconfig` if the
 * configuration file `/etc/ld.so.conf` exists within the
 * installation root directory.
 *
 * It forks a child process to execute `ldconfig` and waits for its
 * completion. The `-r` option is passed to `ldconfig` along with
 * the installation root directory to specify an alternate root for
 * searching libraries.
 *
 * In case of errors during `fork()` or `waitpid()`, a
 * `runtime_error_with_errno` exception is thrown. If `execl()`
 * fails to execute `ldconfig`, an error message is printed to
 * stderr, and the child process exits with failure.
 */
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
      execl(LDCONFIG, LDCONFIG, "-r", root.c_str(), static_cast<char *>(0));
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

/**
 * \brief Prints the footprint of a package file, displaying
 *        detailed file information.
 * \param filename The path to the package file.
 * \throws runtime_error_with_errno If the package file cannot be
 *        opened or read by libarchive.
 * \throws runtime_error If the package file is empty or if reading
 *        the archive fails.
 *
 * \details
 * This function prints a detailed "footprint" of the specified
 * package file, listing each file contained in the archive along
 * with its permissions, owner, group, and special attributes
 * (like symlink target or device major/minor numbers).
 *
 * It performs two passes over the archive:
 * Pass 1: Iterates through the archive to collect file information
 *          (path, symlink target, hardlink target, size, device
 *          numbers, UID, GID, mode) and stores it in a vector of
 *          `file` structures. Skips reading data content of regular
 *          files to optimize for speed.
 * Pass 2: Iterates through the sorted vector of `file` structures
 *          and prints the footprint information for each file in a
 *          format similar to `ls -l`, including:
 *          - File permissions (in string format, e.g.,
 *            "drwxr-xr-x"). For symlinks, always prints
 *            "lrwxrwxrwx".
 *          - User name (or UID if name not found).
 *          - Group name (or GID if name not found).
 *          - File path.
 *          - For symlinks: "-> target".
 *          - For character/block devices: "(major, minor)".
 *          - For empty regular files: "(EMPTY)".
 *
 * The output is printed to standard output, with each file's
 * footprint on a new line.
 */
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

/**
 * \brief Prints the version information of the utility to
 *        standard output.
 *
 * \details
 * This function prints the utility name and version number, as
 * defined by the `VERSION` macro. It also prints compilation
 * options if the utility was compiled with ACL or XATTR support
 * enabled (ENABLE_EXTRACT_ACL, ENABLE_EXTRACT_XATTR).
 */
void
pkgutil::print_version()
  const
{
  cout << utilname << " (pkgutils) " << VERSION << endl;
#if defined(ENABLE_EXTRACT_ACL) || defined(ENABLE_EXTRACT_XATTR)
  cout << "Compiled with options: "
#ifdef ENABLE_EXTRACT_ACL
       << "+acl "
#endif
#ifdef ENABLE_EXTRACT_XATTR
       << "+xattr "
#endif
       << endl;
#endif
}

/**
 * \brief Constructor for the db_lock class, acquires a lock on the
 *        package database directory.
 * \param root The root directory containing the package database.
 * \param exclusive A boolean indicating whether to acquire an
 *                   exclusive lock (true) or a shared lock
 *                   (false).  If false (shared lock requested), a
 *                   non-blocking shared lock is attempted.
 * \throws runtime_error_with_errno If the lock directory cannot be
 *        opened or if locking fails due to system errors.
 * \throws runtime_error If a non-blocking lock is requested
 *                     (exclusive or shared) and the lock cannot
 *                     be immediately acquired.
 *
 * \details
 * This constructor attempts to acquire a lock (exclusive or
 * shared) on the package database directory to prevent concurrent
 * access and data corruption. It opens the directory specified by
 * `PKG_DIR` within the `root` directory and uses `flock()` to
 * acquire the requested lock type.
 *
 * If an exclusive lock is requested and cannot be acquired
 * immediately (because another process holds a lock), or if any
 * error occurs during directory opening or locking (excluding
 * `EWOULDBLOCK` for non-blocking lock attempts), a
 * `runtime_error_with_errno` exception is thrown.
 *
 * If a non-blocking lock is requested (either exclusive or shared)
 * and the lock cannot be acquired immediately (returns
 * `EWOULDBLOCK`), a `runtime_error` is thrown with a message
 * indicating that the database is locked by another process.
 */
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

/**
 * \brief Destructor for the db_lock class, releases the lock on the
 *        package database directory.
 *
 * \details
 * This destructor releases the lock acquired by the `db_lock`
 * constructor on the package database directory. It uses
 * `flock(LOCK_UN)` to release the lock and `closedir()` to close
 * the directory stream.
 *
 * It's crucial to ensure that `db_lock` objects are properly
 * destroyed to release the lock and allow other processes to
 * access the database.
 */
db_lock::~db_lock()
{
  if (dir)
  {
    flock(dirfd(dir), LOCK_UN);
    closedir(dir);
  }
}

/**
 * \brief Converts a file mode (mode_t) to a string representation
 *        (like `ls -l`).
 * \param mode The file mode value (mode_t).
 * \return A string representing the file mode in `ls -l` format
 *          (e.g., "drwxr-xr-x").
 *
 * \details
 * This function takes a file mode value (`mode_t`) and generates a
 * 10-character string representing the file type and permissions,
 * similar to the output of the `ls -l` command.
 *
 * The output string format is:
 * - Character 1: File type (-, d, l, c, b, s, p, ? for unknown).
 * - Characters 2-4: User permissions (rwx, and special
 *                     setuid/setgid/sticky bits as s, S, t, T).
 * - Characters 5-7: Group permissions (rwx, and special
 *                     setuid/setgid/sticky bits as s, S, t, T).
 * - Characters 8-10: Other permissions (rwx, and special
 *                     setuid/setgid/sticky bits as s, S, t, T).
 *
 * Permissions are represented by 'r' (read), 'w' (write), 'x'
 * (execute), and '-' if permission is not granted.  Special bits
 * (setuid, setgid, sticky) are represented by 's', 'S', 't', 'T'.
 */
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

/**
 * \brief Trims redundant slashes from a filename or path.
 * \param filename The filename or path string to trim.
 * \return The trimmed filename string.
 *
 * \details
 * This function removes redundant double slashes ("//") from a
 * filename or path, replacing them with single slashes.  It
 * iterates through the string, finding and replacing instances of
 * "//" until no more are found.
 *
 * Example: "path//to///file" becomes "path/to/file"
 */
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

/**
 * \brief Checks if a file or directory exists.
 * \param filename The path to the file or directory to check.
 * \return True if the file or directory exists, false otherwise.
 *
 * \details
 * This function checks for the existence of a file or directory
 * at the specified path using `lstat()`. It returns true if
 * `lstat()` succeeds (returns 0), indicating that the file or
 * directory exists, and false otherwise.  `lstat()` is used to
 * also check for the existence of symbolic links without
 * dereferencing them.
 */
bool
file_exists(const string& filename)
{
  struct stat buf;
  return !lstat(filename.c_str(), &buf);
}

/**
 * \brief Checks if a file is an empty regular file.
 * \param filename The path to the file to check.
 * \return True if the file is an empty regular file, false
 *          otherwise.
 *
 * \details
 * This function checks if a file is an empty regular file. It uses
 * `lstat()` to get file status information and then checks two
 * conditions:
 * 1. `lstat()` must succeed (return 0), indicating the file exists.
 * 2. The file must be a regular file (`S_ISREG(buf.st_mode)`).
 * 3. The file size (`buf.st_size`) must be 0.
 *
 * Returns true only if all conditions are met, otherwise returns
 * false.
 */
bool
file_empty(const string& filename)
{
  struct stat buf;

  if (lstat(filename.c_str(), &buf) == -1)
    return false;

  return (S_ISREG(buf.st_mode) && buf.st_size == 0);
}

/**
 * \brief Checks if two files are equal in content.
 * \param file1 The path to the first file.
 * \param file2 The path to the second file.
 * \return True if the files are equal in content, false otherwise.
 *
 * \details
 * This function compares the content of two files to determine if
 * they are byte-for-byte identical. It supports comparison for
 * regular files, symbolic links, character devices, and block
 * devices.
 *
 * For regular files, it opens both files, reads their content in
 * blocks, and compares the blocks using `memcmp()`. Files are
 * considered equal if their content is identical and they have the
 * same size.
 *
 * For symbolic links, it reads the link targets using `readlink()`
 * and compares the target paths.
 *
 * For character and block devices, it compares the device numbers
 * (`st_dev`).
 *
 * For other file types or if any error occurs during file
 * operations or stat calls, the function returns false.
 */
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

/**
 * \brief Checks if two files have the same permissions, owner, and
 *        group.
 * \param file1 The path to the first file.
 * \param file2 The path to the second file.
 * \return True if permissions, owner (UID), and group (GID) are
 *          equal for both files, false otherwise.
 *
 * \details
 * This function compares the permissions (mode), owner (UID), and
 * group (GID) of two files to determine if they are identical. It
 * uses `lstat()` to get file status information for both files and
 * then compares the `st_mode`, `st_uid`, and `st_gid` members of
 * the `stat` structures.
 *
 * Returns true only if all three attributes (mode, UID, GID) are
 * equal for both files, otherwise returns false.  Returns false if
 * `lstat()` fails for either file.
 */
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

/**
 * \brief Removes a file or a directory recursively.
 * \param basedir The base directory path.
 * \param filename The relative path of the file or directory to
 *                 remove within `basedir`.
 *
 * \details
 * This function removes a file or directory specified by
 * `filename` relative to `basedir`. If `filename` is not equal to
 * `basedir` (to prevent accidental removal of the base directory
 * itself) and `remove()` call fails, it recursively calls
 * `file_remove` on the parent directory using `dirname()`.  This
 * recursive call is likely intended for directory cleanup after
 * removing files within it, but the logic might have unintended
 * consequences as it attempts to remove parent directories even if
 * the initial `remove()` failed for a file.
 *
 * \warning The recursive behavior of this function, especially
 *          calling `file_remove` on the parent directory after a
 *          failed `remove()` on a file, should be reviewed for
 *          correctness and potential issues.
 */
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
