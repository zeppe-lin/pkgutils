#include "pkgutil.h"
#include "db_lock.h"
#include "pkg_utils.h"
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


//! \def DEFAULT_BYTES_PER_BLOCK
//! \brief Defines the default block size for archive operations.
//!
//! \details
//! This macro defines the default block size, set to 20 * 512
//! bytes (10240 bytes), used for reading data blocks from archives.
#define DEFAULT_BYTES_PER_BLOCK (20 * 512)

using __gnu_cxx::stdio_filebuf;

// Initialize static members of pkgutil class
const char* pkgutil::PKG_DB         = "var/lib/pkg/db";
const char* pkgutil::PKG_DIR        = "var/lib/pkg";
const char* pkgutil::PKG_REJECTED   = "var/lib/pkg/rejected";
const char* pkgutil::VERSION_DELIM  = "#";
const char* pkgutil::PKG_EXT        = ".pkg.tar";
const char* pkgutil::LDCONFIG_CONF  = "/etc/ld.so.conf";
const char* pkgutil::LDCONFIG       = "/sbin/ldconfig"; // or /usr/sbin/ldconfig, check your system
const char* pkgutil::LIB_VERSION    = "1.0";


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
pkgutil::pkgutil(const std::string& name)
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
 *          database.
 * \throws std::runtime_error If the database file cannot be
 *          opened or read.
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
pkgutil::db_open(const std::string& path)
{
    /*
     * Read database.
     */
    root = trim_filename(path + "/");
    const std::string filename = root + PKG_DB;

    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
        throw std::runtime_error(std::string("could not open ") + filename + ": " + strerror(errno));

    stdio_filebuf<char> filebuf(fd, std::ios::in, getpagesize());
    std::istream in(&filebuf);
    if (!in)
        throw std::runtime_error(std::string("could not read ") + filename + ": input stream error");

    while (!in.eof())
    {
        /*
         * Read record.
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

/**
 * \brief Commits changes to the package database, writing it to
 *          disk atomically.
 * \throws std::runtime_error If any file operation during
 *          commit fails.
 *
 * \details
 * This function commits changes made to the in-memory `packages`
 * database by writing the updated database to disk. It implements
 * an atomic commit strategy to prevent database corruption in case
 * of errors or interruptions during the write process.
 *
 * The atomic commit process involves the following steps:
 * 1. Removes any existing incomplete transaction file
 *          (`.incomplete_transaction`).
 * 2. Creates a new database file with a temporary name
 *          (`.incomplete_transaction`) and writes the current in-memory
 *          `packages` database to this new file.
 * 3. Flushes and synchronizes the new database file to disk to
 *          ensure data persistence.
 * 4. Backs up the old database file by renaming it to `.backup`.
 * 5. Renames the new database file (from `.incomplete_transaction`)
 *          to the final database filename, effectively replacing the old
 *          database with the new one.
 *
 * This process ensures that if any error occurs during the commit,
 * the old database remains intact (backed up as `.backup`), and no
 * corrupted or partially written database is left in place.
 */
void
pkgutil::db_commit()
{
    const std::string dbfilename      = root + PKG_DB;
    const std::string dbfilename_new = dbfilename + ".incomplete_transaction";
    const std::string dbfilename_bak = dbfilename + ".backup";

    /*
     * Remove failed transaction (if it exists).
     */
    if (unlink(dbfilename_new.c_str()) == -1 && errno != ENOENT)
        throw std::runtime_error(std::string("could not remove ") +
                                 dbfilename_new + ": " + strerror(errno));

    /*
     * Write new database.
     */
    int fd_new = creat(dbfilename_new.c_str(), 0444);
    if (fd_new == -1)
        throw std::runtime_error(std::string("could not create ") +
                                 dbfilename_new + ": " + strerror(errno));

    stdio_filebuf<char> filebuf_new(fd_new, std::ios::out, getpagesize());
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
     * Move new database into place.
     */
    if (rename(dbfilename_new.c_str(), dbfilename.c_str()) == -1)
        throw std::runtime_error(std::string("could not rename ") +
                                 dbfilename_new + " to " + dbfilename + ": " + strerror(errno));

#ifndef NDEBUG
    std::cerr << packages.size() << " packages written to database" << std::endl;
#endif
}

/**
 * \brief Adds a package to the in-memory database.
 * \param name The name of the package.
 * \param info The package information (`pkginfo_t`) to add.
 *
 * \details
 * This function adds a package and its associated information to
 * the in-memory `packages` map. If a package with the same name
 * already exists, it will be overwritten.
 */
void
pkgutil::db_add_pkg(const std::string& name, const pkginfo_t& info)
{
    packages[name] = info;
}

/**
 * \brief Checks if a package with the given name exists in the
 *          database.
 * \param name The name of the package to search for.
 * \return True if the package exists in the database, false otherwise.
 */
bool
pkgutil::db_find_pkg(const std::string& name)
{
    return (packages.find(name) != packages.end());
}

/**
 * \brief Removes a package and its files from the database and
 *          attempts to delete the files from the filesystem.
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
 *          `ENOTEMPTY` (directory not empty), which might indicate a
 *          shared directory or files placed in the package directory
 *          after installation.
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
     * Don't delete files that still have references.
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
     * Delete the files.
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

/**
 * \brief Removes a package from the database, keeping a specified
 *          list of files.
 * \param name The name of the package to remove.
 * \param keep_list A set of file paths to keep (not remove from
 *          database and filesystem).
 *
 * \details
 * This function removes a package from the `packages` database, but
 * preserves files listed in the `keep_list`. Similar to
 * `db_rm_pkg(const std::string& name)`, it attempts to delete
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
 *          database and filesystem removal.
 * \note File deletion is attempted even if `remove()` fails with
 *          `ENOTEMPTY`, similar to `db_rm_pkg(const std::string& name)`.
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
     * Don't delete files that still have references.
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
     * Delete the files.
     */
    for (std::set<std::string>::const_reverse_iterator
             i = files.rbegin(); i != files.rend(); ++i)
    {
        const std::string filename = root + *i;
        if (file_exists(filename) && remove(filename.c_str()) == -1)
        {
            if (errno == ENOTEMPTY)
                continue;

            const char* msg = strerror(errno);
            std::cerr << utilname << ": could not remove " << filename << ": "
                      << msg << std::endl;
        }
    }
}

/**
 * \brief Removes specific files from the database and attempts to
 *          delete them from the filesystem.
 * \param files A set of file paths to remove from the database and
 *          filesystem.
 * \param keep_list A set of file paths to keep (not remove from
 *          database and filesystem).
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
 *          `ENOTEMPTY`, similar to package removal functions.
 */
void
pkgutil::db_rm_files(std::set<std::string> files, const std::set<std::string>& keep_list)
{
    /*
     * Remove all references.
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
    for (const auto& file : files) {
        std::cerr << file << std::endl;
    }
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
     * Delete the files.
     */
    for (std::set<std::string>::const_reverse_iterator
             i = files.rbegin(); i != files.rend(); ++i)
    {
        const std::string filename = root + *i;
        if (file_exists(filename) && remove(filename.c_str()) == -1)
        {
            if (errno == ENOTEMPTY)
                continue;

            const char* msg = strerror(errno);
            std::cerr << utilname << ": could not remove " << filename << ": "
                      << msg << std::endl;
        }
    }
}

/**
 * \brief Finds file conflicts for a package installation.
 * \param name The name of the package being installed.
 * \param info Package information (`pkginfo_t`) of the package to
 *          be installed.
 * \return A set of file paths that conflict with the package being
 *          installed.
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
std::set<std::string>
pkgutil::db_find_conflicts(const std::string& name, const pkgutil::pkginfo_t&  info)
{
    std::set<std::string> files;

    /*
     * Find conflicting files in database.
     */
    for (packages_t::const_iterator
             i = packages.begin(); i != packages.end(); ++i)
    {
        if (i->first != name)
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
     * Find conflicting files in filesystem.
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
     * Exclude directories.
     */
    std::set<std::string> tmp = files;
    for (std::set<std::string>::const_iterator
             i = tmp.begin(); i != tmp.end(); ++i)
    {
        if ((*i)[i->length() - 1] == '/')
            files.erase(*i);
    }

#ifndef NDEBUG
    std::cerr << "Conflicts phase 3 (directories excluded):" << std::endl;
    std::copy(files.begin(), files.end(),
              std::ostream_iterator<std::string>(std::cerr, "\n"));
    std::cerr << std::endl;
#endif

    /*
     * If this is an upgrade, remove files already owned by this
     * package.
     */
    if (packages.find(name) != packages.end())
    {
        for (std::set<std::string>::const_iterator
                 i  = packages[name].files.begin();
                 i != packages[name].files.end();
                 ++i)
        {
            files.erase(*i);
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

/**
 * \brief Opens a package file and extracts package information
 *          (name, version, file list).
 * \param filename The path to the package file.
 * \return A pair containing the package name (std::string) and package
 *          information (`pkginfo_t`).
 * \throws std::runtime_error If the package file cannot be
 *          opened or read by libarchive.
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
 * `std::runtime_error` is thrown.
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
     * Extract name and version from filename.
     */
    std::string basename(filename, filename.rfind('/') + 1);
    std::string name(basename, 0, basename.find(VERSION_DELIM));
    std::string version(basename, 0, basename.rfind(PKG_EXT));
    version.erase(0,
                  version.find(VERSION_DELIM) == std::string::npos
                      ? std::string::npos
                      : version.find(VERSION_DELIM) + 1);

    if (name.empty() || version.empty())
    {
        throw std::runtime_error(std::string("could not determine name and/or version of ") +
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
        throw std::runtime_error(std::string("could not open ") + filename + ": " + archive_error_string(archive));
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
            throw std::runtime_error(std::string("could not read ") + filename + ": " + archive_error_string(archive));
        }
    }

    if (i == 0)
    {
        if (archive_errno(archive) == 0)
            throw std::runtime_error("empty package");
        else
            throw std::runtime_error(std::string("could not read ") + filename);
    }

    archive_read_free(archive);

    return result;
}

/**
 * \brief Installs a package from a package file.
 * \param filename The path to the package file.
 * \param keep_list A set of file paths to keep if they already
 *          exist on the system.
 * \param non_install_list A set of file paths within the package
 *          that should not be installed.
 * \param upgrade A boolean flag indicating if this is an upgrade
 *          operation.
 * \throws std::runtime_error If the package file cannot be
 *          opened or read by libarchive or if file extraction fails for
 *          a non-upgrade installation.
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
 *      - Checks if the file is in the `non_install_list`; if so, it
 *        is ignored.
 *      - Checks if the file path is in the `keep_list`. If it is and
 *        the file already exists on the system, the file is rejected
 *        (moved to a rejected directory `PKG_REJECTED`) instead of
 *        being installed, and existing files are preserved.
 *      - Extracts the file from the archive to the filesystem using
 *        `archive_read_extract()`, preserving owner, permissions,
 *        and timestamps.
 *      - If file extraction fails and it's not an upgrade
 *        operation, a `std::runtime_error` is thrown.
 *      - For rejected files (files in `keep_list` that already
 *        existed):
 *          - Compares permissions and content (for non-directories) of
 *            the rejected file with the original file. If they are
 *            considered equal (permissions and content match), the
 *            rejected file is removed. Otherwise, a message is
 *            printed indicating that the file was rejected, and the
 *            existing version is kept.
 *
 * \note If file extraction fails during an upgrade, the
 *          installation continues with the rest of the package.
 * \note The function uses `ARCHIVE_EXTRACT_*` flags to control
 *          extraction behavior (owner, permissions, etc.).
 * \note Rejection logic involves moving files to a `PKG_REJECTED`
 *          subdirectory to avoid overwriting existing files in
 *          `keep_list`.
 */
void
pkgutil::pkg_install(const std::string& filename,
                     const std::set<std::string>& keep_list,
                     const std::set<std::string>& non_install_list,
                     bool upgrade)
    const
{
    struct archive*         archive;
    struct archive_entry*   entry;
    unsigned int            i;
    char                    buf[PATH_MAX];
    std::string             absroot;

    archive = archive_read_new();
    INIT_ARCHIVE(archive);

    if (archive_read_open_filename(archive,
                                     filename.c_str(),
                                     DEFAULT_BYTES_PER_BLOCK)
        != ARCHIVE_OK)
    {
        throw std::runtime_error(std::string("could not open ") + filename + ": " + archive_error_string(archive));
    }

    chdir(root.c_str());
    absroot = getcwd(buf, sizeof(buf));

    for (i = 0;
         archive_read_next_header(archive, &entry) == ARCHIVE_OK;
         ++i)
    {
        std::string archive_filename = archive_entry_pathname(entry);

        std::string reject_dir =
            trim_filename(absroot + std::string("/") + std::string(PKG_REJECTED));

        std::string original_filename =
            trim_filename(absroot + std::string("/") + archive_filename);

        std::string real_filename = original_filename;

        /*
         * Check if file is filtered out via INSTALL.
         */
        if (non_install_list.find(archive_filename)
            != non_install_list.end())
        {
            mode_t mode;

            std::cout << utilname << ": ignoring " << archive_filename << std::endl;

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
                trim_filename(reject_dir + std::string("/") + archive_filename);
        }

        archive_entry_set_pathname(entry,
                                     const_cast<char*>(real_filename.c_str()));

        /*
         * Extract file.
         */
        auto flags = ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM
                     | ARCHIVE_EXTRACT_TIME  | ARCHIVE_EXTRACT_UNLINK;

#ifdef ENABLE_EXTRACT_ACL
        flags |= ARCHIVE_EXTRACT_ACL;
#endif
#ifdef ENABLE_EXTRACT_XATTR
        flags |= ARCHIVE_EXTRACT_XATTR;
#endif


        if (archive_read_extract(archive, entry, flags) != ARCHIVE_OK)
        {
            /* If a file fails to install we just print an error message
             * and continue trying to install the rest of the package,
             * unless this is not an upgrade. */
            const char* msg = archive_error_string(archive);
            std::cerr << utilname << ": could not install " +
                         archive_filename << ": " << msg << std::endl;

            if (!upgrade)
            {
                throw std::runtime_error(std::string("extract error: ") + archive_filename +
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
                std::cout << utilname << ": rejecting " << archive_filename
                          << ", keeping existing version" << std::endl;
        }
    }

    if (i == 0)
    {
        if (archive_errno(archive) == 0)
            throw std::runtime_error("empty package");
        else
            throw std::runtime_error(std::string("could not read ") + filename);
    }

    archive_read_free(archive);
}

/**
 * \brief Executes the ldconfig utility to update shared library
 *          cache.
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
 * `std::runtime_error` is thrown. If `execl()`
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
            throw std::runtime_error(std::string("fork() failed: ") + strerror(errno));

        if (pid == 0)
        {
            execl(LDCONFIG, LDCONFIG, "-r", root.c_str(),
                  static_cast<char *>(0));
            const char* msg = strerror(errno);
            std::cerr << utilname << ": could not execute " << LDCONFIG << ": "
                      << msg << std::endl;
            exit(EXIT_FAILURE);
        }
        else
        {
            if (waitpid(pid, 0, 0) == -1)
                throw std::runtime_error(std::string("waitpid() failed: ") + strerror(errno));
        }
    }
}

/**
 * \brief Prints the footprint of a package file, displaying
 *          detailed file information.
 * \param filename The path to the package file.
 * \throws std::runtime_error If the package file cannot be
 *          opened or read by libarchive or if reading the archive fails.
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
pkgutil::pkg_footprint(const std::string& filename)
    const
{
    size_t i;
    struct archive* archive;
    struct archive_entry* entry;

    struct file {
        std::string path;
        std::string soft;
        std::string hard;
        off_t     size;
        dev_t     rdev;
        uid_t     uid;
        gid_t     gid;
        mode_t    mode;
        bool operator < (const struct file& other)
        {
            return (path < other.path);
        }
        bool operator < (const std::string& other)
        {
            return (path < other);
        }
    };

    std::vector<struct file> files;

    /*
     * We first do a run over the archive and remember the modes of
     * regular files.  In the second run, we print the footprint -
     * using the stored modes for hardlinks.
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
        throw std::runtime_error(std::string("could not open ") + filename + ": " + archive_error_string(archive));
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
            throw std::runtime_error(std::string("could not read ") + filename + ": " + archive_error_string(archive));
        }
    }

    if (i == 0)
    {
        if (archive_errno(archive) == 0)
            throw std::runtime_error("empty package");
        else
            throw std::runtime_error(std::string("could not read ") + filename);
    }

    archive_read_free(archive);

    std::sort(files.begin(), files.end());

    for (i = 0; i < files.size(); ++i)
    {
        struct file& file = files[i];

        /*
         * Access permissions.
         */
        if (S_ISLNK(file.mode))
        {
            /* Access permissions on symlinks differ among
             * filesystems, e.g. XFS and ext2 have different.
             *
             * To avoid getting different footprints we always use
             * "lrwxrwxrwx".
             */
            std::cout << "lrwxrwxrwx";
        }
        else
        {
            if (file.hard.length())
            {
                auto it = std::lower_bound(files.begin(), files.end(),
                                             file.hard);
                std::cout << mtos(it->mode);
            }
            else
                std::cout << mtos(file.mode);
        }

        std::cout << '\t';

        /*
         * User.
         */
        struct passwd* pw = getpwuid(file.uid);
        if (pw)
            std::cout << pw->pw_name;
        else
            std::cout << file.uid;

        std::cout << '/';

        /*
         * Group.
         */
        struct group* gr = getgrgid(file.gid);
        if (gr)
            std::cout << gr->gr_name;
        else
            std::cout << file.gid;

        /*
         * Filename.
         */
        std::cout << '\t' << file.path;

        /*
         * Special cases.
         */
        if (S_ISLNK(file.mode))
        {
            /* Symlink. */
            std::cout << " -> " << file.soft;
        }
        else if (S_ISCHR(file.mode) || S_ISBLK(file.mode))
        {
            /* Device. */
            std::cout << " (" << major(file.rdev) << ", " << minor(file.rdev)
                      << ")";
        }
        else if (S_ISREG(file.mode) && file.size == 0)
        {
            /* Empty regular file. */
            std::cout << " (EMPTY)";
        }

        std::cout << '\n';
    }
}

/**
 * \brief Prints the version information of the utility to
 *          standard output.
 *
 * \details
 * This function prints the utility name and version number, as
 * defined by the `LIB_VERSION`. It also prints compilation
 * options if the utility was compiled with ACL or XATTR support
 * enabled (ENABLE_EXTRACT_ACL, ENABLE_EXTRACT_XATTR).
 */
void
pkgutil::print_version()
    const
{
    std::cout << utilname << " (pkgutils) " << LIB_VERSION << std::endl;
#if defined(ENABLE_EXTRACT_ACL) || defined(ENABLE_EXTRACT_XATTR)
    std::cout << "Compiled with options: "
#ifdef ENABLE_EXTRACT_ACL
              << "+acl "
#endif
#ifdef ENABLE_EXTRACT_XATTR
              << "+xattr "
#endif
              << std::endl;
#endif
}
