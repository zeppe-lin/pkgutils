# project metadata
NAME = pkgutils
VERSION = 5.43.1
DESCRIPTION = Package Management Utilities
DIST = ${NAME}-${VERSION}

# paths
PREFIX = /usr
MANPREFIX = ${PREFIX}/share/man

# flags
CPPFLAGS = -D_POSIX_SOURCE -D_GNU_SOURCE -D_LARGEFILE_SOURCE \
	   -D_FILE_OFFSET_BITS=64 -DNDEBUG -DVERSION=\"${VERSION}\"
CXXFLAGS = -Wall -Wextra -pedantic -Wformat -Wformat-security \
	   -Wconversion -Wsign-conversion
LDFLAGS  = -static $(shell pkg-config --libs --static libarchive)

# compiler
CXX = c++
LD  = ${CXX}
