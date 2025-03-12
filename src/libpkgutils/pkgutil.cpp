//! \file  pkgutil.cpp
//! \brief Implementation of the pkgutil class for package management.
//!
//! This file contains the implementation of the `pkgutil` class,
//! providing the core logic for package management operations such as
//! database handling, package installation, removal, and conflict
//! resolution.
//!
//! \copyright See COPYING and COPYRIGHT files for corresponding
//!            information.

#include "pkgutil.h"
#include "db_lock.h"
#include "fs_utils.h"
#include <iostream>
#include <sstream>
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


//! \def DEFAULT_BYTES_PER_BLOCK
//! \brief Defines the default block size for archive operations.
//!
//! \details
//! This macro defines the default block size, set to 20 * 512
//! bytes (10240 bytes), used for reading data blocks from archives
//! by libarchive functions. This block size is used when opening and
//! reading package archive files.
#define DEFAULT_BYTES_PER_BLOCK (20 * 512)

using __gnu_cxx::stdio_filebuf;

//! \name Static Member Initialization
//! \brief Initialization of static member variables of pkgutil class.
//! @{

//! \var pkgutil::PKG_DB
//! \brief Path to the package database file (relative to root).
const char* pkgutil::PKG_DB         = "var/lib/pkg/db";

//! \var pkgutil::PKG_DIR
//! \brief Path to the package directory (relative to root, for locking).
const char* pkgutil::PKG_DIR        = "var/lib/pkg";

//! \var pkgutil::PKG_REJECTED
//! \brief Path to the rejected files directory (relative to root).
const char* pkgutil::PKG_REJECTED   = "var/lib/pkg/rejected";

//! \var pkgutil::VERSION_DELIM
//! \brief Delimiter used in package filenames to separate name and version.
const char* pkgutil::VERSION_DELIM  = "#";

//! \var pkgutil::PKG_EXT
//! \brief Extension for package files.
const char* pkgutil::PKG_EXT        = ".pkg.tar";

//! \var pkgutil::LDCONFIG_CONF
//! \brief Path to the ldconfig configuration file (relative to root).
const char* pkgutil::LDCONFIG_CONF  = "/etc/ld.so.conf";

//! \var pkgutil::LDCONFIG
//! \brief Path to the ldconfig utility executable.
const char* pkgutil::LDCONFIG       = "/sbin/ldconfig";
//                                  or /usr/sbin/ldconfig, check system

//! @}


/*!
 * \brief Constructor for pkgutil class.
 * \param name Name of the utility using pkgutil (e.g., "pkgadd", "pkgrm").
 *
 * \details
 * The constructor initializes the `pkgutil` object with the name of
 * the utility that is using it. It also sets up signal handling to
 * ignore certain signals (SIGHUP, SIGINT, SIGQUIT, SIGTERM) to
 * prevent interruption of package management operations by these
 * signals.
*/
pkgutil::pkgutil(const std::string& name)
    : utilname(name)
{
  /*
   * Ignore signals to prevent interruption of critical operations.
   */
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigaction(SIGHUP,  &sa, 0);
  sigaction(SIGINT,  &sa, 0);
  sigaction(SIGQUIT, &sa, 0);
  sigaction(SIGTERM, &sa, 0);
}

/*!
 * \brief Opens the package database.
 * \param path Root path where the database is located.
 * \throws std::runtime_error If the database cannot be opened or read.
 *
 * \details
 * This method performs the following steps to open the package
 * database:
 * 1.  Constructs the full path to the database file by combining
 *     the provided `path` with the static member `PKG_DB`.
 * 2.  Opens the database file in read-only mode (`O_RDONLY`).
 * 3.  If the file cannot be opened, throws a `std::runtime_error`
 *     indicating the failure and the system error message.
 * 4.  Creates a `stdio_filebuf` associated with the file descriptor
 *     to allow reading from the file as a C++ input stream.
 * 5.  Iterates through the input stream, reading package records.
 * 6.  Each record consists of a package name, package version, and
 *     a list of files. Records are separated by empty lines.
 * 7.  Populates the internal `packages` map with the package
 *     information read from the database.
 *
 * The database is expected to be a text file with each package
 * record formatted as follows:
 * \verbatim
 * packageName
 * packageVersion
 * file1
 * file2
 * ...
 * fileN
 *
 * \endverbatim
 * Records are separated by blank lines.
 */
void
pkgutil::db_open(const std::string& path)
{
  /*
   * Read database from file.
   */
  root = trim_filename(path + "/");
  const std::string filename = root + PKG_DB;

  int fd = open(filename.c_str(), O_RDONLY);
  if (fd == -1)
    throw std::runtime_error(std::string("could not open ") + filename +
                             ": " + strerror(errno));

  stdio_filebuf<char> filebuf(fd, std::ios::in,
                              static_cast<size_t>(getpagesize()));
  std::istream in(&filebuf);
  if (!in)
    throw std::runtime_error(std::string("could not read ") + filename +
                             ": input stream error");

  while (!in.eof())
  {
    /*
     * Read record from database file.
     */
    std::string     name;
    pkginfo_t info;

    getline(in, name);
    getline(in, info.version);

    for (;;)
    {
      std::string file;
      getline(in, file);

      if (file.empty())
          break; /* End of record. */

      info.files.insert(info.files.end(), file);
    }
    if (!info.files.empty())
        packages[name] = info;
  }

#ifndef NDEBUG
  std::cerr << packages.size() << " packages found in database" << std::endl;
#endif
}

/*!
 * \brief Commits changes to the package database.
 *
 * \throws std::runtime_error If database commit fails due to
 *                          write errors or synchronization issues.
 *
 * \details
 * This method implements a transactional commit to update the
 * package database safely. It follows these steps:
 * 1.  Defines filenames for the database, a new database
 *     (incomplete transaction), and a backup.
 * 2.  Removes any existing incomplete transaction file from a
 *     previous failed commit attempt.
 * 3.  Creates a new database file (as incomplete transaction) with
 *     read-only permissions (0444).
 * 4.  Opens the new database file using `stdio_filebuf` and
 *     `std::ostream` for writing.
 * 5.  Iterates through the internal `packages` map and writes each
 *     package record to the new database file in the defined format.
 * 6.  Flushes the output stream to ensure all data is written to the
 *     file buffer.
 * 7.  Checks if the write operation was successful by verifying the
 *     state of the output stream.
 * 8.  Synchronizes the new database file to disk using `fsync()` to
 *     ensure data durability.
 * 9.  Removes any existing backup database file.
 * 10. Creates a hard link from the current database to the backup
 *     database, effectively backing up the old database.
 * 11. Renames the new database (incomplete transaction) to the
 *     actual database filename, completing the commit.
 *
 * Error handling is performed at each step. If any operation fails
 * (e.g., file creation, writing, renaming), a `std::runtime_error`
 * is thrown with an appropriate error message, and the transaction
 * is considered failed. The use of a temporary file and backup
 * minimizes the risk of database corruption during commit operations.
 */
void
pkgutil::db_commit()
{
  const std::string dbfilename     = root + PKG_DB;
  const std::string dbfilename_new = dbfilename + ".incomplete_transaction";
  const std::string dbfilename_bak = dbfilename + ".backup";

  /*
   * Remove failed transaction (if it exists) from previous attempt.
   */
  if (unlink(dbfilename_new.c_str()) == -1 && errno != ENOENT)
    throw std::runtime_error(std::string("could not remove ") +
                             dbfilename_new + ": " + strerror(errno));

  /*
   * Write new database to a temporary file first.
   */
  int fd_new = creat(dbfilename_new.c_str(), 0444);
  if (fd_new == -1)
    throw std::runtime_error(std::string("could not create ") +
                             dbfilename_new + ": " + strerror(errno));

  stdio_filebuf<char> filebuf_new(fd_new, std::ios::out,
                                  static_cast<size_t>(getpagesize()));
  std::ostream db_new(&filebuf_new);

  for (packages_t::const_iterator
           i = packages.begin(); i != packages.end(); ++i)
  {
    if (!i->second.files.empty())
    {
      db_new << i->first << "\n";
      db_new << i->second.version << "\n";
      std::copy(i->second.files.begin(), i->second.files.end(),
                std::ostream_iterator<std::string>(db_new, "\n"));
      db_new << "\n";
    }
  }

  db_new.flush();

  /*
   * Make sure the new database was successfully written.
   */
  if (!db_new)
    throw std::runtime_error(std::string("could not write ") + dbfilename_new);

  /*
   * Synchronize file to disk.
   */
  if (fsync(fd_new) == -1)
    throw std::runtime_error(std::string("could not synchronize ") +
                             dbfilename_new + ": " + strerror(errno));

  /*
   * Relink database backup.
   */
  if (unlink(dbfilename_bak.c_str()) == -1 && errno != ENOENT)
    throw std::runtime_error(std::string("could not remove ") +
                             dbfilename_bak + ": " + strerror(errno));

  if (link(dbfilename.c_str(), dbfilename_bak.c_str()) == -1)
    throw std::runtime_error(std::string("could not create ") +
                             dbfilename_bak + ": " + strerror(errno));

  /*
   * Move new database into place, making the transaction atomic.
   */
  if (rename(dbfilename_new.c_str(), dbfilename.c_str()) == -1)
    throw std::runtime_error(std::string("could not rename ") +
                             dbfilename_new + " to " + dbfilename + ": " +
                             strerror(errno));

#ifndef NDEBUG
  std::cerr << packages.size() << " packages written to database" << std::endl;
#endif
}

/*!
 * \brief Adds a package to the database.
 * \param name Package name.
 * \param info Package information structure (`pkginfo_t`).
 *
 * \details
 * This method adds a new package entry into the internal `packages`
 * map.  The map is used as an in-memory representation of the
 * package database. The actual database file on disk is updated
 * only when `db_commit()` is called. This function simply updates
 * the in-memory database representation.
 */
void
pkgutil::db_add_pkg(const std::string& name, const pkginfo_t& info)
{
  packages[name] = info;
}

/*!
 * \brief Checks if a package exists in the database.
 * \param name Package name to search for.
 * \return True if the package is found, false otherwise.
 *
 * \details
 * This method checks for the existence of a package in the
 * internal `packages` map. It uses the `find()` method of the map
 * to search for the package name. It returns `true` if the package
 * name is found as a key in the `packages` map, indicating that
 * the package is present in the database. Otherwise, it returns
 * `false`.
 */
bool
pkgutil::db_find_pkg(const std::string& name)
{
  return (packages.find(name) != packages.end());
}

/*!
 * \brief Removes a package from the database and deletes its files.
 * \param name Package name to remove.
 *
 * \details
 * This method removes a package from the database and attempts to
 * delete the files associated with it. The removal process is done
 * in several phases:

 * 1.  **Phase 1: Initial File Set**: Retrieves the set of files
 *     belonging to the package being removed from the `packages`
 *     map.
 * 2.  **Phase 2: Exclude Referenced Files**: Iterates through all
 *     other packages in the database. For each file in the initial
 *     file set, it checks if any other package still references
 *     this file. If a file is referenced by another package, it is
 *     removed from the file set to be deleted. This prevents
 *     deleting files that are shared or required by other installed
 *     packages.
 * 3.  **Phase 3: File Deletion**: Iterates through the remaining
 *     files in the file set (in reverse order to handle directory
 *     deletions correctly). For each file, it constructs the full
 *     path, checks if the file exists, and attempts to remove it
 *     using `remove()`. If file removal fails (except for `ENOTEMPTY`
 *     errors which are ignored for directory removal), an error
 *     message is printed to `std::cerr`.

 * After file deletion, the package entry is removed from the
 * `packages` map. The database changes are not committed to disk
 * until `db_commit()` is called.
 *
 * \note Reverse iteration over files is used to ensure directories
 *       are deleted after files within them.
 */
void
pkgutil::db_rm_pkg(const std::string& name)
{
  std::set<std::string> files = packages[name].files;
  packages.erase(name);

#ifndef NDEBUG
  std::cerr << "Removing package phase 1 (all files in package):" << std::endl;
  std::copy(files.begin(), files.end(),
            std::ostream_iterator<std::string>(std::cerr, "\n"));
  std::cerr << std::endl;
#endif

  /*
   * Don't delete files that still have references from other
   * packages.
   */
  for (packages_t::const_iterator
           i = packages.begin(); i != packages.end(); ++i)
  {
    for (std::set<std::string>::const_iterator
             j = i->second.files.begin(); j != i->second.files.end(); ++j)
    {
      files.erase(*j);
    }
  }

#ifndef NDEBUG
  std::cerr << "Removing package phase 2 "
            << "(files that still have references excluded):"
            << std::endl;
  std::copy(files.begin(), files.end(),
            std::ostream_iterator<std::string>(std::cerr, "\n"));
  std::cerr << std::endl;
#endif

  /*
   * Delete the files from filesystem. Reverse iteration is important
   * for directory removal to work correctly (remove files before
   * directories).
   */
  for (std::set<std::string>::const_reverse_iterator
           i = files.rbegin(); i != files.rend(); ++i)
  {
    const std::string filename = root + *i;
    if (file_exists(filename) && remove(filename.c_str()) == -1)
    {
      const char* msg = strerror(errno);
      std::cerr << utilname << ": could not remove " << filename << ": "
                << msg << std::endl;
    }
  }
}

/*!
 * \brief Removes a package from the database, keeping specified files.
 * \param name Package name to remove.
 * \param keep_list Set of files to keep (not delete) during package removal.
 *
 * \details
 * This method is an overloaded version of `db_rm_pkg` that provides
 * the option to keep specific files from being deleted during
 * package removal. It performs the following steps:
 *
 * 1.  **Phase 1: Initial File Set**: Retrieves the set of files
 *     belonging to the package to be removed.
 * 2.  **Phase 2: Exclude Keep List Files**: Removes files present
 *     in the `keep_list` from the initial file set. These are the
 *     files that the user wants to preserve.
 * 3.  **Phase 3: Exclude Referenced Files**: Similar to the base
 *     `db_rm_pkg` method, it iterates through other packages and
 *     removes any files from the file set that are still referenced
 *     by other packages.
 * 4.  **Phase 4: File Deletion**: Deletes the remaining files in the
 *     file set from the filesystem, similar to the base
 *     `db_rm_pkg` method.
 *
 * After file deletion, the package entry is removed from the
 * `packages` map.  Database changes are not committed until
 * `db_commit()` is invoked.
 *
 * \note This overload allows for preserving configuration files or
 *       other specific files during package removal.
 */
void
pkgutil::db_rm_pkg(const std::string& name, const std::set<std::string>& keep_list)
{
  std::set<std::string> files = packages[name].files;
  packages.erase(name);

#ifndef NDEBUG
  std::cerr << "Removing package phase 1 (all files in package):" << std::endl;
  std::copy(files.begin(), files.end(),
            std::ostream_iterator<std::string>(std::cerr, "\n"));
  std::cerr << std::endl;
#endif

  /*
   * Don't delete files found in the keep list.
   */
  for (std::set<std::string>::const_iterator
           i = keep_list.begin(); i != keep_list.end(); ++i)
  {
    files.erase(*i);
  }

#ifndef NDEBUG
  std::cerr << "Removing package phase 2 "
            << "(files that is in the keep list excluded):"
            << std::endl;
  std::copy(files.begin(), files.end(),
            std::ostream_iterator<std::string>(std::cerr, "\n"));
  std::cerr << std::endl;
#endif

  /*
   * Don't delete files that still have references from other
   * packages.
   */
  for (packages_t::const_iterator
           i = packages.begin(); i != packages.end(); ++i)
  {
    for (std::set<std::string>::const_iterator
             j = i->second.files.begin(); j != i->second.files.end(); ++j)
    {
      files.erase(*j);
    }
  }

#ifndef NDEBUG
  std::cerr << "Removing package phase 3 "
            << "(files that still have references excluded):"
            << std::endl;
  std::copy(files.begin(), files.end(),
            std::ostream_iterator<std::string>(std::cerr, "\n"));
  std::cerr << std::endl;
#endif

  /*
   * Delete the files from filesystem. Reverse iteration is important
   * for directory removal to work correctly (remove files before
   * directories).
   */
  for (std::set<std::string>::const_reverse_iterator
           i = files.rbegin(); i != files.rend(); ++i)
  {
    const std::string filename = root + *i;
    if (file_exists(filename) && remove(filename.c_str()) == -1)
    {
      if (errno == ENOTEMPTY)
        continue; // Ignore ENOTEMPTY for directory removal

      const char* msg = strerror(errno);
      std::cerr << utilname << ": could not remove " << filename << ": "
                << msg << std::endl;
    }
  }
}

/*!
 * \brief Removes specified files from the filesystem and database records.
 * \param files Set of files to remove.
 * \param keep_list Set of files to keep (not delete) during file removal.
 *
 * \details
 * This method is designed to remove a specific set of files,
 * potentially across multiple packages, while respecting a list of
 * files to keep.  The process is as follows:
 *
 * 1.  **Phase 1: Remove Database References**: Iterates through all
 *     packages in the database. For each package, it removes
 *     references to any file that is present in the input `files`
 *     set from the package's file list.  This ensures that the
 *     database is updated to reflect the removal of these files.
 * 2.  **Phase 2: Exclude Keep List Files**: Removes files present in
 *     the `keep_list` from the input `files` set. These are the
 *     files that should be preserved.
 * 3.  **Phase 3: File Deletion**: Deletes the remaining files in the
 *     `files` set from the filesystem.  Similar to package removal,
 *     it iterates in reverse order and attempts to remove each file
 *     using `remove()`.  Handles `ENOTEMPTY` errors gracefully and
 *     reports other removal errors to `std::cerr`.
 *
 * This method is useful for scenarios where a set of files needs to
 * be removed independently of package removal, such as cleaning up
 * orphaned or outdated files. Database changes are committed only
 * when `db_commit()` is called.
 *
 * \note Reverse iteration during file deletion is important for
 *       correct directory removal.
 */
void
pkgutil::db_rm_files(std::set<std::string> files, const std::set<std::string>& keep_list)
{
  /*
   * Remove all references to these files from database.
   */
  for (packages_t::iterator
           i = packages.begin(); i != packages.end(); ++i)
  {
    for (std::set<std::string>::const_iterator
             j = files.begin(); j != files.end(); ++j)
    {
      i->second.files.erase(*j);
    }
  }

#ifndef NDEBUG
  std::cerr << "Removing files:" << std::endl;
  for (const auto& file : files)
    std::cerr << file << std::endl;
  std::cerr << std::endl;
#endif

  /*
   * Don't delete files found in the keep list.
   */
  for (std::set<std::string>::const_iterator
           i = keep_list.begin(); i != keep_list.end(); ++i)
  {
    files.erase(*i);
  }

  /*
   * Delete the files from filesystem. Reverse iteration is important
   * for directory removal to work correctly (remove files before
   * directories).
   */
  for (std::set<std::string>::const_reverse_iterator
           i = files.rbegin(); i != files.rend(); ++i)
  {
    const std::string filename = root + *i;
    if (file_exists(filename) && remove(filename.c_str()) == -1)
    {
      if (errno == ENOTEMPTY)
        continue; // Ignore ENOTEMPTY for directory removal

      const char* msg = strerror(errno);
      std::cerr << utilname << ": could not remove " << filename << ": "
                << msg << std::endl;
    }
  }
}

/*!
 * \brief Finds file conflicts for a package.
 * \param name Package name.
 * \param info Package information (`pkginfo_t`) of the package to check.
 * \return Set of conflicting files.
 *
 * \details
 * This method identifies potential file conflicts that would arise
 * from installing a new package. It checks for conflicts in two
 * main phases:
 *
 * 1.  **Phase 1: Database Conflicts**:  Iterates through all
 *     currently installed packages in the database (excluding the
 *     package being installed itself, if it's an upgrade). For each
 *     installed package, it finds the intersection of the files
 *     from the package being installed and the files of the
 *     currently installed package.  Any common files are considered
 *     conflicts and added to the `files` set.
 * 2.  **Phase 2: Filesystem Conflicts**: Iterates through all files
 *     of the package being installed. For each file, it checks if
 *     the file already exists in the filesystem (under the root
 *     directory). If a file exists in the filesystem and was not
 *     already identified as a conflict in Phase 1 (database
 *     conflicts), it is added to the `files` set of conflicts.
 * 3.  **Phase 3: Exclude Directories**:  Removes directory paths
 *     from the set of conflicting files.  The conflict detection
 *     logic primarily focuses on file conflicts, not directory
 *     conflicts. Directories are typically handled by the archive
 *     extraction process.
 * 4.  **Phase 4: Upgrade Exclusion**: If the package being checked
 *     is an upgrade of an already installed package (i.e., a
 *     package with the same name already exists in the database),
 *     files that are already owned by the existing package are
 *     removed from the conflict set.  In an upgrade scenario, files
 *     owned by the previous version are typically replaced and not
 *     considered conflicts.
 *
 * The method returns a `std::set<std::string>` containing the paths
 * of all identified conflicting files. This set can be used to
 * inform the user about potential conflicts before proceeding with
 * package installation.
 */
std::set<std::string>
pkgutil::db_find_conflicts(const std::string& name, const pkgutil::pkginfo_t&  info)
{
  std::set<std::string> files;

  /*
   * Phase 1: Find conflicting files in database.
   */
  for (packages_t::const_iterator
           i = packages.begin(); i != packages.end(); ++i)
  {
    if (i->first != name) // Don't compare package with itself (upgrade case)
    {
      std::set_intersection(info.files.begin(), info.files.end(),
                            i->second.files.begin(), i->second.files.end(),
                            std::inserter(files, files.end()));
    }
  }

#ifndef NDEBUG
  std::cerr << "Conflicts phase 1 (conflicts in database):" << std::endl;
  std::copy(files.begin(), files.end(),
            std::ostream_iterator<std::string>(std::cerr, "\n"));
  std::cerr << std::endl;
#endif

  /*
   * Phase 2: Find conflicting files in filesystem. Add files that
   * exist in filesystem, but not in database conflicts yet.
   */
  for (std::set<std::string>::iterator
           i = info.files.begin(); i != info.files.end(); ++i)
  {
    const std::string filename = root + *i;

    if (file_exists(filename) && files.find(*i) == files.end())
      files.insert(files.end(), *i);
  }

#ifndef NDEBUG
  std::cerr << "Conflicts phase 2 (conflicts in filesystem added):" << std::endl;
  std::copy(files.begin(), files.end(),
            std::ostream_iterator<std::string>(std::cerr, "\n"));
  std::cerr << std::endl;
#endif

  /*
   * Phase 3: Exclude directories from conflict list. We are only
   * interested in file conflicts, not directory conflicts.
   */
  std::set<std::string> tmp = files; // Create a copy for iteration
  for (std::set<std::string>::const_iterator
           i = tmp.begin(); i != tmp.end(); ++i)
  {
    if ((*i)[i->length() - 1] == '/') // Check if it ends with '/' - directory
      files.erase(*i);
  }

#ifndef NDEBUG
  std::cerr << "Conflicts phase 3 (directories excluded):" << std::endl;
  std::copy(files.begin(), files.end(),
            std::ostream_iterator<std::string>(std::cerr, "\n"));
  std::cerr << std::endl;
#endif

  /*
   * Phase 4: If this is an upgrade, remove files already owned by
   * this package from the conflict list.
   */
  if (packages.find(name) != packages.end())
  {
    for (std::set<std::string>::const_iterator
             i  = packages[name].files.begin();
             i != packages[name].files.end();
             ++i)
    {
      files.erase(*i); // Remove files already owned by package being upgraded
    }

#ifndef NDEBUG
    std::cerr << "Conflicts phase 4 "
              << "(files already owned by this package excluded):"
              << std::endl;
    std::copy(files.begin(), files.end(),
              std::ostream_iterator<std::string>(std::cerr, "\n"));
    std::cerr << std::endl;
#endif
  }

  return files;
}

/*!
 * \brief Opens a package file and extracts package information.
 * \param filename Path to the package file.
 * \return Pair containing package name and package information (`pkginfo_t`).
 * \throws std::runtime_error If the package file cannot be opened or read,
 *                          or if the package format is invalid.
 *
 * \details
 * This method opens a package archive file, extracts metadata, and
 * reads the list of files contained within the archive. It performs
 * the following actions:
 *
 * 1.  **Filename Parsing**: Extracts the package name and version
 *     from the provided `filename`. It assumes a filename format
 *     like `package_name#version.pkg.tar`, using `VERSION_DELIM`
 *     ("#") and `PKG_EXT` (".pkg.tar") to parse the name and version.
 *     If name or version cannot be determined from the filename, it
 *     throws a `std::runtime_error`.
 * 2.  **Libarchive Initialization**: Initializes a libarchive read
 *     object using the `INIT_ARCHIVE` macro, which enables support
 *     for gzip, bzip2, xz, lzip, zstd compression and tar format.
 * 3.  **Archive Opening**: Opens the package file using
 *     `archive_read_open_filename()`. If the file cannot be opened
 *     or read by libarchive, it throws a `std::runtime_error` with
 *     the libarchive error message.
 * 4.  **Archive Processing**: Reads through the archive entries
 *     (headers and data) using `archive_read_next_header()`. For
 *     each entry, it extracts the pathname and inserts it into the
 *     `files` set within the `pkginfo_t` structure. It skips
 *     reading data for regular files using `archive_read_data_skip()`.
 * 5.  **Error Checking**: Checks if any entries were read from the
 *     archive. If no entries are found (i.e., empty archive) or if
 *     `archive_errno()` indicates an error after reading, it throws
 *     a `std::runtime_error`.
 * 6.  **Cleanup**: Frees the libarchive read object using
 *     `archive_read_free()`.
 * 7.  **Return Value**: Returns a `std::pair` containing the
 *     extracted package name and the `pkginfo_t` structure, which
 *     includes the version and the set of files from the archive.
 *
 * This method uses libarchive to handle various archive formats and
 * compression methods, providing a robust way to read package file
 * contents.
 */
std::pair<std::string, pkgutil::pkginfo_t>
pkgutil::pkg_open(const std::string& filename)
    const
{
  std::pair<std::string, pkginfo_t> result;
  unsigned int i;
  struct archive* archive;
  struct archive_entry* entry;

  /*
   * Extract name and version from filename based on delimiters.
   */
  std::string basename(filename, filename.rfind('/') + 1); // Extract basename
  std::string name(basename, 0, basename.find(VERSION_DELIM)); // Name up to VERSION_DELIM
  std::string version(basename, 0, basename.rfind(PKG_EXT)); // Version up to PKG_EXT
  version.erase(0, // Erase name part from version, keep only version string
                version.find(VERSION_DELIM) == std::string::npos
                    ? std::string::npos
                    : version.find(VERSION_DELIM) + 1);

  // Error if name or version is empty after parsing
  if (name.empty() || version.empty())
  {
    throw std::runtime_error(std::string("could not determine name and/or version of ") +
                             basename + ": Invalid package name");
  }

  result.first = name;
  result.second.version = version;

  archive = archive_read_new(); // Allocate archive object
  INIT_ARCHIVE(archive);        // Initialize archive for reading

  // Open archive file with libarchive
  if (archive_read_open_filename(archive,
                                 filename.c_str(),
                                 DEFAULT_BYTES_PER_BLOCK)
      != ARCHIVE_OK)
  {
    throw std::runtime_error(std::string("could not open ") + filename +
                             ": " + archive_error_string(archive));
  }

  // Iterate through archive entries (headers)
  for (i = 0;
       archive_read_next_header(archive, &entry) == ARCHIVE_OK;
       ++i)
  {
    result.second.files.insert(result.second.files.end(),
                                archive_entry_pathname(entry)); // Add file path to files set

    mode_t mode = archive_entry_mode(entry); // Get file mode from entry

    // Skip data reading for regular files
    if (   S_ISREG(mode)
        && archive_read_data_skip(archive) != ARCHIVE_OK)
    {
      throw std::runtime_error(std::string("could not read ") + filename +
                               ": " + archive_error_string(archive));
    }
  }

  // Check if any entries were processed. If not, it might be an empty archive
  if (i == 0)
  {
    if (archive_errno(archive) == 0)
      throw std::runtime_error("empty package"); // Empty archive
    else
      throw std::runtime_error(std::string("could not read ") + filename); // Other read error
  }

  archive_read_free(archive); // Free archive object

  return result; // Return package name and info
}

/*!
 * \brief Installs a package from a package file.
 * \param filename Path to the package file.
 * \param keep_list Set of files to keep if conflicts arise.
 * \param non_install_list Set of files to skip during installation.
 * \param upgrade Boolean indicating if this is an upgrade operation.
 * \throws std::runtime_error If package installation fails due to
 *                          archive errors, file extraction problems,
 *                          or other installation issues.
 *
 * \details
 * This method installs a package from a provided package archive
 * file. It performs the following steps:
 *
 * 1.  **Libarchive Initialization**: Initializes a libarchive read
 *     object using `INIT_ARCHIVE`.
 * 2.  **Archive Opening**: Opens the package file for reading using
 *     `archive_read_open_filename()`.
 * 3.  **Directory Change**: Changes the current working directory
 *     to the root directory specified for package operations.
 * 4.  **Absolute Root Path**: Gets the absolute path of the root
 *     directory using `getcwd()`.
 * 5.  **Archive Extraction Loop**: Iterates through each entry in
 *     the archive using `archive_read_next_header()`. For each entry:
 *     a.  **Filename Extraction**: Gets the pathname of the archive
 *         entry.
 *     b.  **Rejected Directory Path Construction**: Constructs the
 *         full path to the "rejected" directory where files are
 *         moved if they conflict or are kept.
 *     c.  **Original and Real Filename Construction**: Constructs
 *         the original target filename (within the root directory)
 *         and initializes the `real_filename` to this original name.
 *     d.  **Non-Install List Check**: Checks if the current
 *         archive entry's filename is present in the `non_install_list`.
 *         If it is, the file is ignored (not installed), a message
 *         is printed, and the process continues to the next entry.
 *     e.  **Rejection Check**: Checks if a file with the same name
 *         already exists at the `real_filename` location AND if the
 *         current archive entry's filename is present in the
 *         `keep_list`. If both conditions are true, it means a
 *         conflict is detected, and the existing file should be
 *         "rejected" (kept, not overwritten). In this case,
 *         `real_filename` is updated to point to a new location
 *         within the "rejected" directory.
 *     f.  **Pathname Update**: Updates the archive entry's pathname
 *         to the `real_filename` using `archive_entry_set_pathname()`.
 *         This ensures that libarchive extracts the file to the
 *         correct (potentially rejected) location.
 *     g.  **Extraction Flags**: Sets extraction flags for
 *         `archive_read_extract()` to control extraction behavior
 *         (owner, permissions, time, unlink, ACLs, XATTRs, if
 *         enabled at compile time).
 *     h.  **File Extraction**: Extracts the file using
 *         `archive_read_extract()`. If extraction fails, an error
 *         message is printed. If it's not an upgrade, a
 *         `std::runtime_error` is thrown to abort installation on
 *         extraction failure. For upgrades, installation continues
 *         despite individual file extraction failures.
 *     i.  **Rejected File Handling**: After extraction, if the
 *         `real_filename` is different from the `original_filename`
 *         (meaning the file was rejected), it checks if the
 *         existing rejected file should be removed.  It compares
 *         permissions and file content (or emptiness) to decide
 *         whether to remove the rejected file. If removal is
 *         deemed appropriate, `file_remove()` is used to remove the
 *         rejected file. Otherwise, a message is printed indicating
 *         that the rejected file is kept.
 * 6.  **Post-Extraction Check**: After processing all archive
 *     entries, checks if any entries were actually processed. If
 *     not, and there was no archive error, it throws a
 *     `std::runtime_error` indicating an empty package. If there was
 *     an archive error, it throws an error indicating the file
 *     could not be read.
 * 7.  **Cleanup**: Frees the libarchive read object using
 *     `archive_read_free()`.
 *
 * This installation process handles file conflicts, file rejection
 * (keeping existing files), and optional file skipping based on
 * lists.  It uses libarchive for robust archive handling and
 * extracts files with preservation of metadata (permissions,
 * ownership, etc.).
*/
void
pkgutil::pkg_install(const std::string& filename,
                     const std::set<std::string>& keep_list,
                     const std::set<std::string>& non_install_list,
                     bool upgrade)
    const
{
  struct archive*      archive;
  struct archive_entry* entry;
  unsigned int         i;
  char                 buf[PATH_MAX];
  std::string          absroot;

  archive = archive_read_new(); // Allocate archive object
  INIT_ARCHIVE(archive);        // Initialize archive for reading

  // Open archive file with libarchive
  if (archive_read_open_filename(archive,
                                 filename.c_str(),
                                 DEFAULT_BYTES_PER_BLOCK)
      != ARCHIVE_OK)
  {
    throw std::runtime_error(std::string("could not open ") + filename +
                             ": " + archive_error_string(archive));
  }

  chdir(root.c_str()); // Change directory to root for extraction
  absroot = getcwd(buf, sizeof(buf)); // Get absolute root path

  // Iterate through archive entries for extraction
  for (i = 0;
       archive_read_next_header(archive, &entry) == ARCHIVE_OK;
       ++i)
  {
    std::string archive_filename = archive_entry_pathname(entry); // Get entry path

    // Construct path to rejected directory
    std::string reject_dir =
        trim_filename(absroot + std::string("/") + std::string(PKG_REJECTED));

    // Construct original filename (target install path)
    std::string original_filename =
        trim_filename(absroot + std::string("/") + archive_filename);

    std::string real_filename = original_filename; // Initially real == original

    /*
     * Check if file is filtered out via INSTALL file (non_install_list).
     */
    if (non_install_list.find(archive_filename)
        != non_install_list.end())
    {
      mode_t mode;

      std::cout << utilname << ": ignoring " << archive_filename << std::endl;

      mode = archive_entry_mode(entry);

      if (S_ISREG(mode)) // Skip data for regular files
          archive_read_data_skip(archive);

      continue; // Skip to next archive entry
    }

    /*
     * Check if file should be rejected (kept, not overwritten).
     */
    if (   file_exists(real_filename)
        && keep_list.find(archive_filename) != keep_list.end())
    {
      // Construct path to rejected file
      real_filename =
          trim_filename(reject_dir + std::string("/") + archive_filename);
    }

    // Set the actual pathname for extraction (could be original or rejected path)
    archive_entry_set_pathname(entry,
                               const_cast<char*>(real_filename.c_str()));

    /*
     * Extract file with specified flags (permissions, ownership, etc.).
     */
    auto flags = ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM
               | ARCHIVE_EXTRACT_TIME  | ARCHIVE_EXTRACT_UNLINK;

#ifdef ENABLE_EXTRACT_ACL
    flags |= ARCHIVE_EXTRACT_ACL; // Enable ACL extraction if compiled with ACL support
#endif
#ifdef ENABLE_EXTRACT_XATTR
    flags |= ARCHIVE_EXTRACT_XATTR; // Enable XATTR extraction if compiled with XATTR support
#endif


    if (archive_read_extract(archive, entry, flags) != ARCHIVE_OK)
    {
      /* If a file fails to install we just print an error message
       * and continue trying to install the rest of the package,
       * unless this is not an upgrade - then we abort. */
      const char* msg = archive_error_string(archive);
      std::cerr << utilname << ": could not install " +
                       archive_filename << ": " << msg << std::endl;

      if (!upgrade) // Abort installation if not upgrade
      {
        throw std::runtime_error(std::string("extract error: ") +
                                  archive_filename + ": " + msg);
      }
      continue; // Continue to next archive entry in upgrade case
    }

    /*
     * Check if file was rejected (installed to rejected dir).
     */
    if (real_filename != original_filename)
    {
      bool remove_file = false;
      mode_t mode = archive_entry_mode(entry);

      /* directory case */
      if (S_ISDIR(mode))
      {
        remove_file = permissions_equal(real_filename,
                                        original_filename);
      }
      /* other files case */
      else
      {
        remove_file =
                permissions_equal(real_filename, original_filename)
             && (   file_empty(real_filename)
                 || file_equal(real_filename, original_filename));
      }

      /* remove rejected file if conditions met or signal existence */
      if (remove_file)
        file_remove(reject_dir, real_filename); // Remove rejected file
      else
        std::cout << utilname << ": rejecting " << archive_filename
                  << ", keeping existing version" << std::endl; // Signal rejection
    }
  }

  // Post-extraction checks (empty package, archive errors)
  if (i == 0)
  {
    if (archive_errno(archive) == 0)
      throw std::runtime_error("empty package"); // No entries in archive
    else
      throw std::runtime_error(std::string("could not read ") + filename); // Archive read error
  }

  archive_read_free(archive); // Free archive object
}

/*!
 * \brief Executes ldconfig to update shared library cache.
 *
 * \details
 * This method is responsible for running the `ldconfig` utility to
 * update the shared library cache. It is typically called after
 * installing or removing packages that contain shared libraries.
 * The method performs the following steps:
 *
 * 1.  **Configuration File Check**: Checks if the `ldconfig`
 *     configuration file (`LDCONFIG_CONF`) exists within the root
 *     directory. `ldconfig` is only executed if this configuration
 *     file exists, as it indicates that shared libraries are managed
 *     on the system.
 * 2.  **Fork Process**: If the configuration file exists, it forks a
 *     child process using `fork()`.
 * 3.  **Execute ldconfig in Child Process**: In the child process,
 *     it attempts to execute `ldconfig` using `execl()`. The `-r`
 *     option is passed to `ldconfig` along with the root directory
 *     to specify an alternate root for scanning shared libraries.
 *     If `execl()` fails, an error message is printed to
 *     `std::cerr`, and the child process exits with failure status.
 * 4.  **Wait for Child Process in Parent**: In the parent process,
 *     it waits for the child process to complete using `waitpid()`.
 *     If `waitpid()` fails, a `std::runtime_error` is thrown.
 *
 * This method uses `fork()` and `execl()` to execute `ldconfig` in
 * a separate process to avoid blocking the package management
 * utility and to ensure that `ldconfig` runs with the correct
 * environment and permissions. Error handling is included for fork
 * and waitpid system calls, as well as for the case where `ldconfig`
 * execution fails within the child process.
 */
void
pkgutil::ldconfig()
    const
{
  /*
   * Only execute ldconfig if /etc/ld.so.conf exists in root.
   */
  if (file_exists(root + LDCONFIG_CONF))
  {
    pid_t pid = fork(); // Fork process to run ldconfig

    if (pid == -1)
      throw std::runtime_error(std::string("fork() failed: ") + strerror(errno));

    if (pid == 0)
    {
      // Child process: execute ldconfig
      execl(LDCONFIG, LDCONFIG, "-r", root.c_str(),
            static_cast<char *>(0)); // Execute ldconfig with root path
      const char* msg = strerror(errno);
      std::cerr << utilname << ": could not execute " << LDCONFIG << ": "
                << msg << std::endl;
      exit(EXIT_FAILURE); // Exit child process on execl failure
    }
    else
    {
      // Parent process: wait for child
      if (waitpid(pid, 0, 0) == -1)
        throw std::runtime_error(std::string("waitpid() failed: ") + strerror(errno));
    }
  }
}

/*!
 * \brief Prints the footprint of a package file.
 * \param filename Path to the package file.
 * \throws std::runtime_error If the package file cannot be opened or read.
 *
 * \details
 * This method generates and prints a footprint of the contents of a
 * package file. The footprint provides a detailed listing of each
 * entry in the archive, including permissions, ownership, filename,
 * and special file type information (symlinks, device nodes, empty
 * files). The method performs a two-pass process:
 *
 * **Pass 1: Archive Scan and Metadata Collection**:
 * 1.  Initializes a libarchive read object using `INIT_ARCHIVE`.
 * 2.  Opens the package file using `archive_read_open_filename()`.
 * 3.  Iterates through each entry in the archive using
 *     `archive_read_next_header()`.
 * 4.  For each entry, it extracts relevant metadata such as
 *     pathname, symlink target, hardlink target, size, device IDs,
 *     user and group IDs, and mode (permissions). This metadata is
 *     stored in a `file` structure and added to a `files` vector.
 * 5.  For regular files, it skips reading the file data using
 *     `archive_read_data_skip()`.
 * 6.  After scanning all entries, it frees the libarchive object.
 *
 * **Pass 2: Footprint Printing**:
 * 1.  Sorts the `files` vector alphabetically by path.
 * 2.  Iterates through the sorted `files` vector.
 * 3.  For each `file` structure:
 *     a.  **Permissions**: Prints the file permissions in symbolic
 *         form (e.g., "rw-r--r--"). For symlinks, it always prints
 *         "lrwxrwxrwx" to ensure consistent footprint across
 *         filesystems. For hardlinks, it retrieves the permissions
 *         from the linked file's metadata (obtained in Pass 1).
 *     b.  **User/Group**: Prints the username and group name based
 *         on the UID and GID, or the numeric UID/GID if the name
 *         cannot be resolved using `getpwuid()` and `getgrgid()`.
 *     c.  **Filename**: Prints the file path.
 *     d.  **Special File Info**: Appends special information for
 *         symlinks ("-> target"), device nodes ("(major, minor)"),
 *         and empty regular files ("(EMPTY)") to the output line.
 *     e.  Prints a newline character to complete the footprint line
 *         for the current file.
 *
 * The output is printed to `std::cout` and is intended to provide a
 * human-readable summary of the package file's contents and
 * metadata, useful for package inspection and verification.
 */
void
pkgutil::pkg_footprint(const std::string& filename)
    const
{
  size_t i;
  struct archive* archive;
  struct archive_entry* entry;

  // Structure to hold file information for footprinting
  struct file {
    std::string path;  // File path
    std::string soft;  // Symlink target
    std::string hard;  // Hardlink target
    off_t       size;  // File size
    dev_t       rdev;  // Device ID
    uid_t       uid;   // User ID
    gid_t       gid;   // Group ID
    mode_t      mode;  // File mode

    // Overload less-than operator for sorting files by path
    bool operator < (const struct file& other)
    {
      return (path < other.path);
    }
    bool operator < (const std::string& other)
    {
      return (path < other);
    }
  };

  std::vector<struct file> files; // Vector to store file info

  /*
   * Pass 1: Scan archive to collect metadata for footprint.
   * We need to store modes for regular files for hardlink handling
   * in the second pass.
   */
  archive = archive_read_new(); // Allocate archive object
  INIT_ARCHIVE(archive);        // Initialize archive for reading

  // Open archive file with libarchive
  if (archive_read_open_filename(archive,
                                 filename.c_str(),
                                 DEFAULT_BYTES_PER_BLOCK)
      != ARCHIVE_OK)
  {
    throw std::runtime_error(std::string("could not open ") + filename +
                             ": " + archive_error_string(archive));
  }

  // Iterate through archive entries (headers) - Pass 1
  for (i = 0;
       archive_read_next_header(archive, &entry) == ARCHIVE_OK;
       ++i)
  {
    struct file file; // File info struct for current entry
    const char* s;

    if ((s = archive_entry_pathname(entry)))
      file.path = s; // Get path

    if ((s = archive_entry_symlink(entry)))
      file.soft = s; // Get symlink target

    if ((s = archive_entry_hardlink(entry)))
      file.hard = s; // Get hardlink target

    file.size = archive_entry_size(entry);   // Get size
    file.rdev = archive_entry_rdev(entry);   // Get device ID
    file.uid  = static_cast<uid_t>(archive_entry_uid(entry)); // Get UID
    file.gid  = static_cast<gid_t>(archive_entry_gid(entry)); // Get GID
    file.mode = archive_entry_mode(entry);   // Get mode

    files.push_back(file); // Add file info to vector

    if (S_ISREG(file.mode) && archive_read_data_skip(archive)) // Skip data for regular files
    {
      throw std::runtime_error(std::string("could not read ") + filename +
                               ": " + archive_error_string(archive));
    }
  }

  // Error check after pass 1
  if (i == 0)
  {
    if (archive_errno(archive) == 0)
      throw std::runtime_error("empty package"); // Empty archive
    else
      throw std::runtime_error(std::string("could not read ") + filename); // Read error
  }

  archive_read_free(archive); // Free archive object after pass 1

  std::sort(files.begin(), files.end()); // Sort files by path for consistent output

  /*
   * Pass 2: Print footprint using collected metadata.
   */
  for (i = 0; i < files.size(); ++i)
  {
    struct file& file = files[i]; // Get file info

    /*
     * Access permissions.
     */
    if (S_ISLNK(file.mode))
    {
      /* Access permissions on symlinks can vary. Force consistent output. */
      std::cout << "lrwxrwxrwx"; // Consistent symlink permissions
    }
    else
    {
      if (file.hard.length())
      {
        // For hardlinks, get mode from the linked file's info
        auto it = std::lower_bound(files.begin(), files.end(), file.hard);
        std::cout << mtos(it->mode); // Convert mode to string
      }
      else
        std::cout << mtos(file.mode); // Convert mode to string
    }

    std::cout << '\t'; // Separator

    /*
     * User.
     */
    struct passwd* pw = getpwuid(file.uid); // Get user name from UID
    if (pw)
      std::cout << pw->pw_name; // Print user name
    else
      std::cout << file.uid;    // Print UID if name not found

    std::cout << '/'; // Separator

    /*
     * Group.
     */
    struct group* gr = getgrgid(file.gid); // Get group name from GID
    if (gr)
      std::cout << gr->gr_name; // Print group name
    else
      std::cout << file.gid;    // Print GID if name not found

    /*
     * Filename.
     */
    std::cout << '\t' << file.path; // Print file path

    /*
     * Special cases (symlinks, devices, empty files) in footprint output.
     */
    if (S_ISLNK(file.mode))
    {
      /* Symlink target. */
      std::cout << " -> " << file.soft; // Print symlink target
    }
    else if (S_ISCHR(file.mode) || S_ISBLK(file.mode))
    {
      /* Device nodes (character or block devices). */
      std::cout << " (" << major(file.rdev) << ", " << minor(file.rdev)
                << ")"; // Print major and minor device numbers
    }
    else if (S_ISREG(file.mode) && file.size == 0)
    {
      /* Empty regular file. */
      std::cout << " (EMPTY)"; // Indicate empty regular file
    }

    std::cout << '\n'; // Newline for next file footprint entry
  }
}

/*!
 * \brief Prints the versions of the pkgutil library and utility name.
 */
void
pkgutil::print_version()
    const
{
  std::ostringstream version_stream;

  version_stream << utilname << " from pkgutils " << VERSION
                 << " (libpkgutils " << LIB_VERSION;

  std::vector<std::string> options;

#ifdef ENABLE_EXTRACT_ACL
  options.push_back("acl");
#endif

#ifdef ENABLE_EXTRACT_XATTR
  options.push_back("xattr");
#endif

#ifndef NDEBUG
  options.push_back("debug");
#endif

  if (!options.empty())
  {
    version_stream << ": ";

    // join options with commas
    for (size_t i = 0; i < options.size(); ++i)
    {
      version_stream << options[i];
      if (i < options.size() - 1)
        version_stream << ", ";
    }
  }

  version_stream << ")";
  std::cout << version_stream.str() << std::endl;
}

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.
