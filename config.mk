# pkgutils version
VERSION = 5.41

# paths
PREFIX  = /usr/local
BINDIR  = $(PREFIX)/sbin
MANDIR  = $(PREFIX)/share/man
ETCDIR  = /etc

# flags
CXXFLAGS += -Wall -Wextra -pedantic
CPPFLAGS += -D_POSIX_SOURCE -D_GNU_SOURCE -DVERSION=\"$(VERSION)\" \
            -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DNDEBUG
LDFLAGS += --static $(shell pkg-config --libs --static libarchive)

# compiler and linker
CXX ?= g++
LD = $(CXX)
