# project metadata
NAME        = pkgutils
VERSION     = 6.3

# paths
PREFIX      = /usr
MANPREFIX   = $(PREFIX)/share/man
BASHCOMPDIR = $(PREFIX)/share/bash-completion/completions
VIMFILESDIR = $(PREFIX)/share/vim/vimfiles

# flags
CPPFLAGS    = -D_POSIX_SOURCE -D_GNU_SOURCE -D_LARGEFILE_SOURCE \
              -D_FILE_OFFSET_BITS=64 -DNDEBUG -DVERSION=\"$(VERSION)\"
CXXFLAGS    = -std=c++0x -pedantic -Wall -Wextra \
              $(shell pkg-config --cflags libpkgcore)
LDFLAGS     = --static
LDLIBS      = $(shell pkg-config --static --libs libpkgcore)

# compiler and linker
CXX         = c++
LD          = $(CXX)
