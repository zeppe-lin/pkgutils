# project metadata
NAME        = pkgutils
VERSION     = 6.3

# paths
PREFIX      = /usr
MANPREFIX   = $(PREFIX)/share/man
BASHCOMPDIR = $(PREFIX)/share/bash-completion/completions
VIMFILESDIR = $(PREFIX)/share/vim/vimfiles

# Uncomment to preserve packages' Access Control Lists by `pkgadd`.
# See `acl(5)` for more information.
#
# ** `libarchive` must be compiled with enabled ACL support. **
#
# NOTE:
#   The ACL support in libarchive is written against the POSIX1e
#   draft, which was never officially approved and varies quite a bit
#   accross platforms.  Worse, some systems have completely non-POSIX
#   acl functions.
#
#ACL        = -DENABLE_EXTRACT_ACL

# Uncomment to preserve packages' Extended attributes by `pkgadd`.
# See `xattr(7)` for more information.
#
# ** `libarchive` must be compiled with enabled xattrs support. **
#
#XATTR      = -DENABLE_EXTRACT_XATTR

# flags
CPPFLAGS    = -D_POSIX_SOURCE -D_GNU_SOURCE -D_LARGEFILE_SOURCE \
              -D_FILE_OFFSET_BITS=64 -DNDEBUG -DVERSION=\"$(VERSION)\" \
              $(ACL) $(XATTR)
CXXFLAGS    = -std=c++0x -pedantic -Wall -Wextra
LDFLAGS     = -larchive

# compiler and linker
CXX         = c++
LD          = $(CXX)
