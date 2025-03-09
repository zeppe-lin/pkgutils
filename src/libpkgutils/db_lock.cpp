#include "db_lock.h"
#include "pkg_utils.h" // for runtime_error and file_exists if needed, but better to throw std::runtime_error directly and remove dependency if possible.
#include "pkgutil.h" // pkgutil::PKG_DIR
#include <stdexcept> // for std::runtime_error
#include <string>
#include <dirent.h>
#include <sys/file.h>
#include <unistd.h>
#include <cerrno>
#include <iostream> // for cerr in debug, consider removing in final version
#include <cstring>


/**
 * \brief Constructor for the db_lock class, acquires a lock on the
 *          package database directory.
 * \param root The root directory containing the package database.
 * \param exclusive A boolean indicating whether to acquire an
 *          exclusive lock (true) or a shared lock
 *          (false).  If false (shared lock requested), a
 *          non-blocking shared lock is attempted.
 * \throws std::runtime_error If the lock directory cannot be
 *          opened or if locking fails due to system errors, or if a non-blocking lock fails.
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
 * `std::runtime_error` is thrown.
 *
 * If a non-blocking lock is requested (either exclusive or shared)
 * and the lock cannot be acquired immediately (returns
 * `EWOULDBLOCK`), a `std::runtime_error` is thrown with a message
 * indicating that the database is locked by another process.
 */
db_lock::db_lock(const std::string& root, bool exclusive)
    : dir(nullptr)
{
    const std::string dirname = trim_filename(root + std::string("/") + pkgutil::PKG_DIR); // Assuming PKG_DIR is in pkgutil.h

    if (!(dir = opendir(dirname.c_str()))) {
        throw std::runtime_error(std::string("could not read directory ") + dirname + ": " + strerror(errno));
    }

    if (flock(dirfd(dir),
              (exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK) {
            closedir(dir);
            dir = nullptr;
            throw std::runtime_error(
                "package database is currently locked by another process");
        } else {
            closedir(dir);
            dir = nullptr;
            throw std::runtime_error(std::string("could not lock directory ") + dirname + ": " + strerror(errno));
        }
    }
}

/**
 * \brief Destructor for the db_lock class, releases the lock on the
 *          package database directory.
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
        dir = nullptr;
    }
}
