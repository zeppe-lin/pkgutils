.POSIX:
.SUFFIXES: .cpp .o

include ../../config.mk
include ../../extra/flags-extra.mk
include config.mk

LIB_SRC = db_lock.cpp fs_utils.cpp pkgutil.cpp
LIB_HDR = db_lock.h   fs_utils.h   pkgutil.h   libpkgutils.h
LIB_OBJ = db_lock.o   fs_utils.o   pkgutil.o

LIB_TARGET = $(LIB_NAME).a

.cpp.o:
	$(CXX) -c $< $(CPPFLAGS) $(CXXFLAGS)

all: $(LIB_TARGET)

$(LIB_TARGET): $(LIB_OBJ)
	ar rcs $@ $^

clean:
	rm -f $(LIB_OBJ) $(LIB_TARGET)

.PHONY: all clean
