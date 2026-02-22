/*!
 * \file  db_lock.h
 * \brief Declaration of the db_lock class for database locking.
 *
 * This header file defines the `db_lock` class, which provides
 * RAII-style locking for the package database directory to prevent
 * concurrent access.
 *
 * \copyright See COPYING for license terms and COPYRIGHT for notices.
 */

#ifndef DB_LOCK_H
#define DB_LOCK_H

#include <string>
#include <dirent.h>

/*!
 * \class db_lock
 * \brief Provides locking mechanism for the package database
 *        directory.
 *
 * The `db_lock` class uses RAII (Resource Acquisition Is
 * Initialization) to manage locking of the package database
 * directory.  It acquires a lock in the constructor and releases it
 * in the destructor, ensuring that the database is locked only for
 * the duration of an operation.
 *
 * This class supports both exclusive and shared locks to control
 * access concurrency.
*/
class db_lock {
public:
  /*!
   * \brief Constructor for db_lock, acquires a lock on the database.
   * \param root Root directory where the package database is located.
   * \param exclusive Boolean indicating whether to acquire an
   *                  exclusive lock (true) or a shared lock (false).
   * \throws std::runtime_error If locking fails or directory cannot
   *                            be opened.
   */
  db_lock(const std::string& root, bool exclusive = true);

  /*!
   * \brief Destructor for db_lock, releases the lock.
   */
  ~db_lock();

private:
  //! \var dir
  //! \brief Directory stream for the locked database directory.
  DIR* dir;
};

#endif // DB_LOCK_H
