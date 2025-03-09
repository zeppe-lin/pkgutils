#include "pkg_utils.h"
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <cstring>
#include <libgen.h>

/**
 * \brief Converts a file mode (mode_t) to a string representation
 *          (like `ls -l`).
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
 *          setuid/setgid/sticky bits as s, S, t, T).
 * - Characters 5-7: Group permissions (rwx, and special
 *          setuid/setgid/sticky bits as s, S, t, T).
 * - Characters 8-10: Other permissions (rwx, and special
 *          setuid/setgid/sticky bits as s, S, t, T).
 *
 * Permissions are represented by 'r' (read), 'w' (write), 'x'
 * (execute), and '-' if permission is not granted.  Special bits
 * (setuid, setgid, sticky) are represented by 's', 'S', 't', 'T'.
 */
std::string
mtos(mode_t mode)
{
    std::string s;

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
        case S_IXUSR:          s += 'x'; break;
        case S_ISUID:          s += 'S'; break;
        case S_IXUSR | S_ISUID: s += 's'; break;
        default:               s += '-'; break;
    }

    /*
     * Group permissions.
     */
    s += (mode & S_IRGRP) ? 'r' : '-';
    s += (mode & S_IWGRP) ? 'w' : '-';
    switch (mode & (S_IXGRP | S_ISGID))
    {
        case S_IXGRP:          s += 'x'; break;
        case S_ISGID:          s += 'S'; break;
        case S_IXGRP | S_ISGID: s += 's'; break;
        default:               s += '-'; break;
    }

    /*
     * Other permissions.
     */
    s += (mode & S_IROTH) ? 'r' : '-';
    s += (mode & S_IWOTH) ? 'w' : '-';
    switch (mode & (S_IXOTH | S_ISVTX))
    {
        case S_IXOTH:          s += 'x'; break;
        case S_ISVTX:          s += 'T'; break;
        case S_IXOTH | S_ISVTX: s += 't'; break;
        default:               s += '-'; break;
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
std::string
trim_filename(const std::string& filename)
{
    std::string search("//");
    std::string result = filename;

    for (std::string::size_type
             pos  = result.find(search);
             pos != std::string::npos;
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
file_exists(const std::string& filename)
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
file_empty(const std::string& filename)
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
file_equal(const std::string& file1, const std::string& file2)
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
        std::ifstream f1(file1.c_str());
        std::ifstream f2(file2.c_str());

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
 *          group.
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
permissions_equal(const std::string& file1, const std::string& file2)
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
 *          remove within `basedir`.
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
file_remove(const std::string& basedir, const std::string& filename)
{
    if (filename != basedir && !remove(filename.c_str()))
    {
        char* path = strdup(filename.c_str());
        file_remove(basedir, dirname(path));
        free(path);
    }
}
