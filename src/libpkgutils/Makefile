.POSIX:
.SUFFIXES: .cpp .o

include ../../config.mk

LIB_NAME = libpkgutils
LIB_VERSION = 0.1

LIB_SRC = pkg_utils.cpp pkgutil.cpp db_lock.cpp
LIB_HDR = pkg_utils.h   pkgutil.h   db_lock.h   libpkgutils.h
LIB_OBJ = pkg_utils.o   pkgutil.o   db_lock.o

LIB_TARGET = $(LIB_NAME).a

.cpp.o:
	$(CXX) -c $< $(CPPFLAGS) $(CXXFLAGS)

all: $(LIB_TARGET)

$(LIB_TARGET): $(LIB_OBJ)
	ar rcs $@ $^

clean:
	rm -f $(LIB_OBJ) $(LIB_TARGET)

.PHONY: all clean
