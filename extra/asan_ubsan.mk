######################################################################
# ASan and UBSan flags.                                              #
######################################################################

# includes and libs
INCS     =
LIBS     = $(shell pkg-config --libs libarchive) -lasan -lubsan

# flags
CPPFLAGS = -D_POSIX_SOURCE -D_GNU_SOURCE -D_LARGEFILE_SOURCE \
           -D_FILE_OFFSET_BITS=64 -DVERSION=\"$(VERSION)\"
CXXFLAGS = -std=c++0x -O0 -ggdb3 -fno-omit-frame-pointer \
           -fsanitize=address \
           -fsanitize=leak \
           -fsanitize=undefined \
           -fsanitize-recover=address
LDFLAGS  = $(CXXFLAGS) $(LIBS)
