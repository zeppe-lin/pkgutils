# project metadata
NAME        = pkgutils
VERSION     = 6.0.1
DIST        = ${NAME}-${VERSION}

# paths
PREFIX      = /usr
MANPREFIX   = ${PREFIX}/share/man
BASHCOMPDIR = ${PREFIX}/share/bash-completion/completions

# flags
CPPFLAGS    = -D_POSIX_SOURCE -D_GNU_SOURCE -D_LARGEFILE_SOURCE \
	      -D_FILE_OFFSET_BITS=64 -DNDEBUG -DVERSION=\"${VERSION}\"
ifeq (${DEBUG}, 1)
CXXFLAGS    = -ggdb3 -fno-omit-frame-pointer -fsanitize=address \
	      -fsanitize=leak -fsanitize=undefined -fsanitize-recover=address
LDFLAGS     = $(shell pkg-config --libs libarchive) ${CXXFLAGS} -lasan -lubsan
else
CXXFLAGS    = -Wall -Wextra -pedantic -Wformat -Wformat-security \
	      -Wconversion -Wsign-conversion
LDFLAGS     = -static $(shell pkg-config --libs --static libarchive)
endif
