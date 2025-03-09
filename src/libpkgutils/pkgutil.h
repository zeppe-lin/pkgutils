//! \file  pkgutil.h
//! \brief Declaration of the pkgutil class for package management.
//!
//! This header file defines the `pkgutil` class, which provides
//! core functionalities for package management, including database
//! operations, package installation, removal, conflict detection,
//! and archive handling.
//!
//! \copyright See COPYING and COPYRIGHT files for corresponding
//!            information.

#ifndef PKGUTIL_H
#define PKGUTIL_H

#include <string>
#include <set>
#include <vector>
#include <utility> // For std::pair
#include <map>

class db_lock; // Forward declaration

/*!
 * \class pkgutil
 * \brief Provides core package management functionalities.
 *
 * The `pkgutil` class encapsulates the logic for managing software
 * packages. It handles database interactions, package file
 * operations (opening, installing, footprinting), conflict
 * resolution, and system integration tasks like running `ldconfig`.
 *
 * It uses a database to track installed packages and their files,
 * and interacts with libarchive for handling package archive files.
 */
class pkgutil {
public:
  //! \typedef files_t
  //! \brief Type definition for a set of filenames (strings).
  typedef std::set<std::string> files_t;

  /*!
   * \struct pkginfo_t
   * \brief Structure to hold package information.
   *
   * This structure stores information about a package, including
   * its version and the set of files it owns.
   */
  struct pkginfo_t {
      //! \var version
      //! \brief Package version string.
      std::string version;
      //! \var files
      //! \brief Set of files belonging to the package.
      files_t files;
  };

  //! \typedef packages_t
  //! \brief Type definition for a map of package names to pkginfo_t.
  using packages_t = std::map<std::string, pkginfo_t>;


  /*!
   * \brief Constructor for pkgutil class.
   * \param name Name of the utility using pkgutil (e.g., "pkgadd", "pkgrm").
   */
  pkgutil(const std::string& name);

  /*!
   * \brief Opens the package database.
   * \param path Root path where the database is located.
   * \throws std::runtime_error If the database cannot be opened or read.
   *
   * \details
   * This method opens and reads the package database file from the
   * specified root path. It loads package information into the
   * internal `packages` map for subsequent operations.
   */
  void db_open(const std::string& path);

  /*!
   * \brief Commits changes to the package database.
   *
   * \throws std::runtime_error If database commit fails due to
   *                          write errors or synchronization issues.
   *
   * \details
   * This method writes the current state of the `packages` map
   * back to the package database file, ensuring data persistence.
   * It implements a safe commit mechanism involving a temporary
   * file and backup to minimize data corruption in case of errors.
   */
  void db_commit();

  /*!
   * \brief Adds a package to the database.
   * \param name Package name.
   * \param info Package information structure (`pkginfo_t`).
   *
   * \details
   * This method adds a new package record to the internal
   * `packages` map. Note that this method does not directly
   * write to the database file; `db_commit()` must be called to
   * persist changes.
   */
  void db_add_pkg(const std::string& name, const pkginfo_t& info);

  /*!
   * \brief Checks if a package exists in the database.
   * \param name Package name to search for.
   * \return True if the package is found, false otherwise.
   */
  bool db_find_pkg(const std::string& name);

  /*!
   * \brief Removes a package from the database and deletes its files.
   * \param name Package name to remove.
   *
   * \details
   * This method removes a package record from the database and
   * attempts to delete all files associated with the package from
   * the filesystem. It handles file deletion carefully, avoiding
   * deletion of files that are also part of other installed
   * packages.
   */
  void db_rm_pkg(const std::string& name);

  /*!
   * \brief Removes a package from the database, keeping specified files.
   * \param name Package name to remove.
   * \param keep_list Set of files to keep (not delete) during package removal.
   *
   * \details
   * This overload of `db_rm_pkg` allows for selective file
   * retention during package removal. Files listed in `keep_list`
   * will not be deleted even if they belong to the package being removed.
   */
  void db_rm_pkg(const std::string& name, const std::set<std::string>& keep_list);

  /*!
   * \brief Removes specified files from the filesystem and database records.
   * \param files Set of files to remove.
   * \param keep_list Set of files to keep (not delete) during file removal.
   *
   * \details
   * This method removes a set of files from the filesystem, taking
   * into account a `keep_list` of files that should not be
   * deleted. It also updates the package database to remove
   * references to the deleted files from package records.
   */
  void db_rm_files(std::set<std::string> files, const std::set<std::string>& keep_list);

  /*!
   * \brief Finds file conflicts for a package.
   * \param name Package name.
   * \param info Package information (`pkginfo_t`) of the package to check.
   * \return Set of conflicting files.
   *
   * \details
   * This method detects potential file conflicts by comparing the
   * files of the package being added with files already present in
   * the database and filesystem. It identifies files that would
   * be overwritten or conflict with existing installations.
   */
  std::set<std::string> db_find_conflicts(const std::string& name, const pkginfo_t& info);

  /*!
   * \brief Opens a package file and extracts package information.
   * \param filename Path to the package file.
   * \return Pair containing package name and package information (`pkginfo_t`).
   * \throws std::runtime_error If the package file cannot be opened or read,
   *                          or if the package format is invalid.
   *
   * \details
   * This method opens a package archive file, reads its contents
   * using libarchive, and extracts package name, version, and the
   * list of files contained within the package.
   */
  std::pair<std::string, pkginfo_t> pkg_open(const std::string& filename) const;

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
   * This method installs a package from a given package file. It
   * uses libarchive to extract files from the package archive to
   * the filesystem, respecting permissions and ownership. It also
   * handles conflict resolution based on the `keep_list` and
   * `non_install_list`.
   */
  void pkg_install(const std::string& filename,
                   const std::set<std::string>& keep_list,
                   const std::set<std::string>& non_install_list,
                   bool upgrade) const;

  /*!
   * \brief Executes ldconfig to update shared library cache.
   *
   * \details
   * This method runs the `ldconfig` utility (if available) to
   * update the shared library cache after package installation or
   * removal, ensuring that the system recognizes newly installed
   * or removed shared libraries.
   */
  void ldconfig() const;

  /*!
   * \brief Prints the footprint of a package file.
   * \param filename Path to the package file.
   * \throws std::runtime_error If the package file cannot be opened or read.
   *
   * \details
   * This method reads a package file and prints a footprint
   * representation of its contents, including file permissions,
   * ownership, and paths. This is useful for inspecting package
   * contents without installing them.
  */
  void pkg_footprint(const std::string& filename) const;

  /*!
   * \brief Prints the versions of the pkgutil library and utility name.
   */
  void print_version() const;

  /*!
   * \brief Getter for the packages map (read-only).
   * \return A const reference to the internal packages map.
   *
   * \details
   * Provides read-only access to the internal map of installed
   * packages and their information. This allows other parts of the
   * utilities to query the package database.
   */
  const packages_t& getPackages() const {
    return packages;
  }


  //! \name Static Constants
  //! @{

  //! \var PKG_DB
  //! \brief Path to the package database file (relative to root).
  static const char* PKG_DB;

  //! \var PKG_DIR
  //! \brief Path to the package directory (relative to root, for locking).
  static const char* PKG_DIR;

  //! \var PKG_REJECTED
  //! \brief Path to the rejected files directory (relative to root).
  static const char* PKG_REJECTED;

  //! \var VERSION_DELIM
  //! \brief Delimiter used in package filenames to separate name and version.
  static const char* VERSION_DELIM;

  //! \var PKG_EXT
  //! \brief Extension for package files.
  static const char* PKG_EXT;

  //! \var LDCONFIG_CONF
  //! \brief Path to the ldconfig configuration file (relative to root).
  static const char* LDCONFIG_CONF;

  //! \var LDCONFIG
  //! \brief Path to the ldconfig utility executable.
  static const char* LDCONFIG;

  //! @}

private:
  //! \var utilname
  //! \brief Name of the utility using this pkgutil instance.
  std::string utilname;
  //! \var root
  //! \brief Root directory for package operations.
  std::string root;
  //! \var packages
  //! \brief Map storing package information, indexed by package name.
  packages_t packages;
};

/*!
 * \def INIT_ARCHIVE(ar)
 * \brief Initializes a libarchive read object for common archive formats.
 * \param ar The archive read object (`struct archive*`) to initialize.
 *
 * \details
 * This macro simplifies the initialization of a libarchive read
 * object by enabling support for various compression filters and
 * the tar format.  It enables gzip, bzip2, xz, lzip, and zstd
 * decompression filters, as well as tar archive format support.
 *
 * This macro should be used to initialize `struct archive*` objects
 * before using them to read package archives.
 */
#define INIT_ARCHIVE(ar)                      \
    archive_read_support_filter_gzip((ar));   \
    archive_read_support_filter_bzip2((ar));  \
    archive_read_support_filter_xz((ar));     \
    archive_read_support_filter_lzip((ar));   \
    archive_read_support_filter_zstd((ar));   \
    archive_read_support_format_tar((ar))


#endif // PKGUTIL_H

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.
