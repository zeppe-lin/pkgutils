# pkgutils version
VERSION = 5.42

# paths
PREFIX     = /usr/local
MANPREFIX  = ${PREFIX}/share/man
SYSCONFDIR = ${PREFIX}/etc

# flags
CXXFLAGS = -Wall -Wextra -pedantic
CPPFLAGS = -D_POSIX_SOURCE -D_GNU_SOURCE -D_LARGEFILE_SOURCE \
	   -D_FILE_OFFSET_BITS=64 -DNDEBUG \
	   -DSYSCONFDIR=\"${SYSCONFDIR}\" -DVERSION=\"${VERSION}\"
LDFLAGS  = -static $(shell pkg-config --libs --static libarchive)

# compiler
CXX = c++
LD  = ${CXX}
