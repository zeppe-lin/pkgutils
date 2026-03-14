.POSIX:
.SUFFIXES: .cpp .o

include ../config.mk
#include ../extra/flags-extra.mk
#include ../extra/flags-sanitizer.mk

OBJS = pkgutil.o pkgadd.o pkgrm.o pkgchk.o pkginfo.o
BIN1 = pkgchk pkginfo
BIN8 = pkgadd pkgrm

all: $(BIN1) $(BIN8)

.cpp.o:
	$(CXX) -c $< $(CPPFLAGS) $(CXXFLAGS)

pkgadd: pkgadd.o
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

pkgrm: pkgrm.o
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

pkginfo: pkginfo.o
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

pkgchk: pkgchk.o
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

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

.PHONY: all install uninstall clean
