# See COPYING and COPYRIGHT files for corresponding information.

# pkgutils version if undefined
VERSION = 5.41

# paths
PREFIX  = /usr/local
BINDIR  = ${PREFIX}/sbin
MANDIR  = ${PREFIX}/share/man

# flags
CXXFLAGS = -Wall -Wextra -pedantic
CPPFLAGS = -D_POSIX_SOURCE -D_GNU_SOURCE -DVERSION=\"${VERSION}\" \
           -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DNDEBUG
LDFLAGS  = -static $(shell pkg-config --libs --static libarchive)

# vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
# End of file.
