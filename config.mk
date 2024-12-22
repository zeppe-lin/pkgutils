# project metadata
NAME        = pkgutils
VERSION     = 6.0.2

# paths
PREFIX      = /usr
MANPREFIX   = $(PREFIX)/share/man
BASHCOMPDIR = $(PREFIX)/share/bash-completion/completions
VIMFILESDIR = $(PREFIX)/share/vim/vimfiles

# default compiler flags
CPPFLAGS    = -D_POSIX_SOURCE -D_GNU_SOURCE -D_LARGEFILE_SOURCE \
              -D_FILE_OFFSET_BITS=64 -DNDEBUG -DVERSION=\"$(VERSION)\"
CXXFLAGS    = -std=c++0x -Wall -Wextra -pedantic
LDFLAGS     = -static $(shell pkg-config --static --libs libarchive)
