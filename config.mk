# project metadata
NAME        = pkgutils
VERSION     = 6.0.2
DIST        = ${NAME}-${VERSION}

# paths
PREFIX      = /usr
MANPREFIX   = ${PREFIX}/share/man
BASHCOMPDIR = ${PREFIX}/share/bash-completion/completions
VIMFILESDIR = ${PREFIX}/share/vim/vimfiles

# flags
CPPFLAGS    = -D_POSIX_SOURCE -D_GNU_SOURCE -D_LARGEFILE_SOURCE \
	      -D_FILE_OFFSET_BITS=64 -DNDEBUG -DVERSION=\"${VERSION}\"
ifeq (${DEBUG}, 1)
CXXFLAGS    = -std=c++11 -ggdb3 -fno-omit-frame-pointer -fsanitize=address \
	      -fsanitize=leak -fsanitize=undefined -fsanitize-recover=address
LDFLAGS     = $(shell pkg-config --libs libarchive) ${CXXFLAGS} -lasan -lubsan
else
CXXFLAGS    = -std=c++11 -Wall -Wextra -pedantic -Wformat -Wformat-security \
	      -Wconversion -Wsign-conversion
LDFLAGS     = -static $(shell pkg-config --libs --static libarchive)
endif
