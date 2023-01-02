# pkgutils version
VERSION = 5.41

# flags
CXXFLAGS = -Wall -Wextra -pedantic
CPPFLAGS = -D_POSIX_SOURCE -D_GNU_SOURCE -D_LARGEFILE_SOURCE \
	   -D_FILE_OFFSET_BITS=64 -DNDEBUG -DVERSION=\"${VERSION}\"
LDFLAGS  = -static $(shell pkg-config --libs --static libarchive)

# compiler
CXX = c++
LD  = ${CXX}
