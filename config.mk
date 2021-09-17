NAME      = pkgutils
VERSION   = 5.40.7

BINDIR    = /usr/sbin
MANDIR    = /usr/share/man
ETCDIR    = /etc

CXX      ?= g++
LD        = $(CXX)

CXXFLAGS  += -Wall -Wextra -pedantic

CPPFLAGS  = -D_POSIX_SOURCE -D_GNU_SOURCE -DVERSION=\"$(VERSION)\" \
            -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DNDEBUG

#LDFLAGS  += -static $(shell pkg-config --libs --static libarchive)
LDFLAGS   += -static -larchive -lacl -llzma -lzstd -llz4 -lbz2 -lz
#LDFLAGS  += -larchive

# End of file.
