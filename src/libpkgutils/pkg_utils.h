#ifndef PKG_UTILS_H
#define PKG_UTILS_H

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

std::string mtos(mode_t mode);
std::string trim_filename(const std::string& filename);
bool file_exists(const std::string& filename);
bool file_empty(const std::string& filename);
bool file_equal(const std::string& file1, const std::string& file2);
bool permissions_equal(const std::string& file1, const std::string& file2);
void file_remove(const std::string& basedir, const std::string& filename);


#endif // PKG_UTILS_H
