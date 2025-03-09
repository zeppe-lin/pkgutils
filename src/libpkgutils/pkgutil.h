#ifndef PKGUTIL_H
#define PKGUTIL_H

#include <string>
#include <set>
#include <vector>
#include <utility> // For std::pair
#include <map>

class db_lock; // Forward declaration

class pkgutil {
public:
    typedef std::set<std::string> files_t;

    struct pkginfo_t {
        std::string version;
        files_t files;
    };

    //typedef std::map<std::string, pkginfo_t> packages_t;

    pkgutil(const std::string& name);

    void db_open(const std::string& path);
    void db_commit();
    void db_add_pkg(const std::string& name, const pkginfo_t& info);
    bool db_find_pkg(const std::string& name);
    void db_rm_pkg(const std::string& name);
    void db_rm_pkg(const std::string& name, const std::set<std::string>& keep_list);
    void db_rm_files(std::set<std::string> files, const std::set<std::string>& keep_list);
    std::set<std::string> db_find_conflicts(const std::string& name, const pkginfo_t& info);
    std::pair<std::string, pkginfo_t> pkg_open(const std::string& filename) const;
    void pkg_install(const std::string& filename,
                     const std::set<std::string>& keep_list,
                     const std::set<std::string>& non_install_list,
                     bool upgrade) const;
    void ldconfig() const;
    void pkg_footprint(const std::string& filename) const;
    void print_version() const;

    using packages_t = std::map<std::string, pkginfo_t>;

    // Public getter method to access the packages map (read-only)
    const packages_t& getPackages() const { return packages; }

    static const char* PKG_DB;         //!< Package database file
    static const char* PKG_DIR;        //!< Package directory (for locking)
    static const char* PKG_REJECTED;   //!< Rejected files directory
    static const char* VERSION_DELIM;  //!< Version delimiter in package names
    static const char* PKG_EXT;        //!< Package file extension
    static const char* LDCONFIG_CONF;  //!< ldconfig configuration file path
    static const char* LDCONFIG;       //!< Path to ldconfig utility
    static const char* LIB_VERSION;    //!< Library version string

private:
    std::string utilname;
    std::string root;
    packages_t packages;
};

//! \def INIT_ARCHIVE(ar)
//! \brief Initializes a libarchive read object for common
//!          archive formats.
//! \param ar The archive read object (`struct archive*`) to
//!          initialize.
//!
//! \details
//! This macro simplifies the initialization of a libarchive read
//! object by enabling support for various compression filters and
//! the tar format.  It enables gzip, bzip2, xz, lzip, and zstd
//! decompression filters, as well as tar archive format support.
#define INIT_ARCHIVE(ar)                                                 \
  archive_read_support_filter_gzip((ar));                             \
  archive_read_support_filter_bzip2((ar));                            \
  archive_read_support_filter_xz((ar));                               \
  archive_read_support_filter_lzip((ar));                             \
  archive_read_support_filter_zstd((ar));                            \
  archive_read_support_format_tar((ar))


#endif // PKGUTIL_H
