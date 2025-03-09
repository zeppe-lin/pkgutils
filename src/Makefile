.POSIX:
.SUFFIXES: .cpp .o

include ../config.mk

OBJS = pkgadd.o pkginfo.o pkgrm.o pkgutil.o
BIN1 = pkginfo
BIN8 = pkgadd pkgrm

LIB_DIR      = ./libpkgutils
LIB_TARGET   = $(LIB_DIR)/libpkgutils.a
LIB_CPPFLAGS = -I$(LIB_DIR)
LIB_LDFLAGS  = -L$(LIB_DIR)

all: $(BIN1) $(BIN8)

.cpp.o:
	$(CXX) -c $< $(LIB_CPPFLAGS) $(CPPFLAGS) $(CXXFLAGS)

lint:

$(LIB_TARGET):
	cd $(LIB_DIR) && $(MAKE)

pkgadd: pkgadd.o $(LIB_TARGET)
	$(CXX) $^ $(LDFLAGS) -o $@

pkgrm: pkgrm.o $(LIB_TARGET)
	$(CXX) $^ $(LDFLAGS) -o $@

pkginfo: pkginfo.o $(LIB_TARGET)
	$(CXX) $^ $(LDFLAGS) -o $@

#$(BIN1) $(BIN8): $(OBJS) $(LIB_TARGET)
#	$(CXX) $(OBJS) $(LDFLAGS) -o $@

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/sbin
	cp -f $(BIN1) $(DESTDIR)$(PREFIX)/bin
	cp -f $(BIN8) $(DESTDIR)$(PREFIX)/sbin
	cd $(DESTDIR)$(PREFIX)/bin && chmod 0755 $(BIN1)
	cd $(DESTDIR)$(PREFIX)/sbin && chmod 0755 $(BIN8)

uninstall:
	cd $(DESTDIR)$(PREFIX)/bin  && rm -f $(BIN1)
	cd $(DESTDIR)$(PREFIX)/sbin && rm -f $(BIN8)

clean:
	rm -f $(OBJS) $(BIN1) $(BIN8)
	cd $(LIB_DIR) && $(MAKE) clean

.PHONY: all lint install uninstall clean
