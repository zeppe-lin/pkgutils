//! \file  pkgutil.h
//! \brief pkgutil class and helpers definition.
//!        See COPYING and COPYRIGHT files for corresponding information.

#pragma once

#include <string>
#include <set>
#include <map>
#include <iostream>
#include <stdexcept>
#include <cerrno>
#include <cstring>

#include <sys/types.h>
#include <dirent.h>

#include "pathnames.h"

using namespace std;

/**
 * \class pkgutil
 * \brief Base class for package utility tools.
 *
 * \details
 * This class provides common functionality for package management
 * utilities, including database operations and package file handling.
 * It is designed to be subclassed by specific package utilities
 * (e.g., install, remove, etc.).
 */
class pkgutil
{
public:

  /**
   * \struct pkginfo_t
   * \brief Structure to hold package information.
   *
   * \details
   * Contains the package version and a set of files belonging to the
   * package.
   */
  struct pkginfo_t
  {
    string      version;
    set<string> files;
  };

  /**
   * \typedef packages_t
   * \brief Type definition for the packages database.
   *
   * \details
   * A map where the key is the package name and the value is its
   * pkginfo_t.
   */
  typedef map<string, pkginfo_t> packages_t;

 /**
   * \brief Constructor for pkgutil class.
   * \param name Utility name.
   */
  explicit pkgutil(const string& name);

  /**
   * \brief Virtual destructor for pkgutil class.
   */
  virtual ~pkgutil() {}

  /**
   * \brief Pure virtual function to run the utility.
   * \param argc Argument count.
   * \param argv Argument vector.
   */
  virtual void run(int argc, char** argv) = 0;

  /**
   * \brief Pure virtual function to print help message.
   */
  virtual void print_help() const = 0;

  /**
   * \brief Prints the version of the utility.
   */
  virtual void print_version() const;

protected:
  /**
   * \name Database Operations
   *
   * \{ */

  /**
   * \brief Opens the package database.
   * \param path Path to the database directory.
   */
  void db_open(const string& path);

  /**
   * \brief Commits changes to the package database.
   */
  void db_commit();

  /**
   * \brief Adds a package to the database.
   * \param name Package name.
   * \param info Package information structure.
   */
  void db_add_pkg(const string& name, const pkginfo_t& info);

  /**
   * \brief Finds a package in the database.
   * \param name Package name to find.
   * \return True if package is found, false otherwise.
   */
  bool db_find_pkg(const string& name);

  /**
   * \brief Removes a package from the database.
   * \param name Package name to remove.
   */
  void db_rm_pkg(const string& name);

  /**
   * \brief Removes a package from the database, keeping specified
   *        files.
   * \param name Package name to remove.
   * \param keep_list Set of files to keep (not remove from database).
   */
  void db_rm_pkg(const string& name, const set<string>& keep_list);

  /**
   * \brief Removes files from the database.
   * \param files Set of files to remove from database.
   * \param keep_list Set of files to keep (not remove from database).
   */
  void db_rm_files(set<string> files, const set<string>& keep_list);

  /**
   * \brief Finds file conflicts for a package before installation.
   * \param name Package name.
   * \param info Package information structure.
   * \return Set of conflicting file paths.
   */
  set<string> db_find_conflicts(const string& name, const pkginfo_t& info);

  /** \} */ // End of Database Operations


  /**
   * \name Tar.gz Package Operations
   *
   * \{ */

  /**
   * \brief Opens a package file (tar.gz) and extracts package
   *        information.
   * \param filename Path to the package file.
   * \return Pair containing package name and package information.
   * \throws runtime_error if archive operation fails.
   */
  pair<string, pkginfo_t> pkg_open(const string& filename) const;

  /**
   * \brief Installs a package from a package file.
   * \param filename Path to the package file.
   * \param keep_list Set of files to keep during installation (not
   *                  used in install, likely for remove/upgrade).
   * \param non_install_files Set of files in the package that should
   *                          not be installed.
   * \param upgrade Flag indicating if it is an upgrade operation.
   * \throws runtime_error if installation fails.
   */
  void pkg_install(const string& filename, const set<string>& keep_list,
                   const set<string>& non_install_files, bool upgrade) const;

  /**
   * \brief Prints the footprint of a package file (package information).
   * \param filename Path to the package file.
   */
  void pkg_footprint(const string& filename) const;

  /**
   * \brief Runs ldconfig to update shared library links.
   */
  void ldconfig() const;

  /** \} */ // End of Tar.gz Package Operations

//protected: ???

  //! Utility name.
  string utilname;

  //! Packages database (in-memory representation).
  packages_t packages;

  //! Root directory for operations (e.g., installation root).
  string root;
}; // class pkgutil


/**
 * \class db_lock
 * \brief Class for managing database lock.
 *
 * \details
 * Provides a simple locking mechanism for database operations to
 * prevent concurrent access and data corruption.  Uses directory
 * locking.
 */
class db_lock
{
public:

  /**
   * \brief Constructor for db_lock.
   * \param root Root directory where the lock file resides.
   * \param exclusive True for exclusive lock, false for shared (not
   *                  used in this context).
   */
  db_lock(const string& root, bool exclusive);

  /**
   * \brief Destructor for db_lock, releases the lock.
   */
  ~db_lock();

private:
  //! Directory stream, used for lock.
  DIR* dir;
}; // class db_lock


/**
 * \class runtime_error_with_errno
 * \brief Runtime error exception with errno information.
 *
 * \details
 * Extends std::runtime_error to include system error number (errno)
 * information in the exception message, making error reporting more
 * informative.
 */
class runtime_error_with_errno : public runtime_error
{
public:
  /**
   * \brief Constructor from a message string.
   * \param msg Error message string.
   * \throws std::runtime_error
   */
  explicit runtime_error_with_errno(const string& msg) throw()
    : runtime_error(msg + string(": ") + strerror(errno)) {}

  /**
   * \brief Constructor from a message string and an errno value.
   * \param msg Error message string.
   * \param e Errno value to include in the message.
   * \throws std::runtime_error
   */
  explicit runtime_error_with_errno(const string& msg, int e) throw()
    : runtime_error(msg + string(": ") + strerror(e)) {}
}; // class runtime_error_with_errno


/**
 * \name Utility Functions
 *
 * \{ */

/**
 * \brief Converts file mode to string representation.
 * \param mode File mode (mode_t).
 * \return String representation of file mode (e.g., "drwxr-xr-x").
 */
string mtos(mode_t mode);

/**
 * \brief Trims redundant and trailing slashes from a filename.
 * \param filename Filename string.
 * \return Trimmed filename string.
 */
string trim_filename(const string& filename);

/**
 * \brief Checks if a file or directory exists.
 * \param filename Path to the file or directory.
 * \return True if file/directory exists, false otherwise.
 */
bool file_exists(const string& filename);

/**
 * \brief Checks if a file is empty.
 * \param filename Path to the file.
 * \return True if file is empty, false otherwise.
 */
bool file_empty(const string& filename);

/**
 * \brief Checks if two files are content-equal.
 * \param file1 Path to the first file.
 * \param file2 Path to the second file.
 * \return True if files are content-equal, false otherwise.
 */
bool file_equal(const string& file1, const string& file2);

/**
 * \brief Checks if two files have the same permissions.
 * \param file1 Path to the first file.
 * \param file2 Path to the second file.
 * \return True if permissions are equal, false otherwise.
 */
bool permissions_equal(const string& file1, const string& file2);

/**
 * \brief Removes a file or an empty directory.
 * \param basedir Base directory for the file.
 * \param filename Relative path of the file within the base directory.
 */
void file_remove(const string& basedir, const string& filename);

/** \} */ // End of Utility Functions

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.
