include $(CURDIR)/../config.mk
#include $(CURDIR)/../extra/asan_ubsan.mk
#include $(CURDIR)/../extra/extra_warns.mk

OBJS = main.o pkgadd.o pkginfo.o pkgrm.o pkgutil.o
BIN1 = pkginfo
BIN8 = pkgadd pkgrm

all: $(BIN1) $(BIN8)

pkgadd: $(OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@

pkginfo pkgrm: pkgadd
	ln -sf pkgadd $@

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/sbin
	cp -f pkgadd $(DESTDIR)$(PREFIX)/sbin
	chmod 0755 $(DESTDIR)$(PREFIX)/sbin/pkgadd
	ln -sf pkgadd $(DESTDIR)${PREFIX}/sbin/pkgrm
	ln -sf ../sbin/pkgadd $(DESTDIR)$(PREFIX)/bin/pkginfo

uninstall:
	cd $(DESTDIR)$(PREFIX)/bin  && rm -f $(BIN1)
	cd $(DESTDIR)$(PREFIX)/sbin && rm -f $(BIN8)

clean:
	rm -f $(OBJS) $(BIN1) $(BIN8)

.PHONY: all install uninstall clean
