//! \file  fs_utils.h
//! \brief Declaration of file system utility functions.
//!
//! This header file declares a collection of utility functions
//! used throughout the package management utilities. These functions
//! provide functionalities such as file mode conversion, filename
//! manipulation, file existence checks, file comparison, and file
//! removal, specifically focusing on file system operations.
//!
//! \copyright See COPYING and COPYRIGHT files for corresponding
//!            information.

#ifndef FS_UTILS_H
#define FS_UTILS_H

#include <limits.h>
#include <sys/param.h>

#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>

//! \def DEFAULT_BYTES_PER_BLOCK
//! \brief Defines the default block size for archive operations.
//!
//! \details
//! This macro defines the default block size, set to 20 * 512
//! bytes (10240 bytes), used for reading data blocks from archives.
#define DEFAULT_BYTES_PER_BLOCK (20 * 512)

/*!
 * \brief Converts a file mode (mode_t) to a string representation (like `ls -l`).
 * \param mode The file mode value (mode_t).
 * \return A string representing the file mode in `ls -l` format (e.g., "drwxr-xr-x").
 */
std::string mtos(mode_t mode);

/*!
 * \brief Trims redundant slashes from a filename or path.
 * \param filename The filename or path string to trim.
 * \return The trimmed filename string.
 */
std::string trim_filename(const std::string& filename);

/*!
 * \brief Checks if a file or directory exists.
 * \param filename The path to the file or directory to check.
 * \return True if the file or directory exists, false otherwise.
 */
bool file_exists(const std::string& filename);

/*!
 * \brief Checks if a file is an empty regular file.
 * \param filename The path to the file to check.
 * \return True if the file is an empty regular file, false otherwise.
 */
bool file_empty(const std::string& filename);

/*!
 * \brief Checks if two files are equal in content.
 * \param file1 The path to the first file.
 * \param file2 The path to the second file.
 * \return True if the files are equal in content, false otherwise.
 */
bool file_equal(const std::string& file1, const std::string& file2);

/*!
 * \brief Checks if two files have the same permissions, owner, and group.
 * \param file1 The path to the first file.
 * \param file2 The path to the second file.
 * \return True if permissions, owner (UID), and group (GID) are equal for both files, false otherwise.
 */
bool permissions_equal(const std::string& file1, const std::string& file2);

/*!
 * \brief Removes a file or a directory recursively.
 * \param basedir The base directory path.
 * \param filename The relative path of the file or directory to remove within `basedir`.
 */
void file_remove(const std::string& basedir, const std::string& filename);


#endif // FS_UTILS_H

// vim: sw=2 ts=2 sts=2 et cc=72 tw=70
// End of file.