.POSIX:

include config.mk

SRCS = $(wildcard *.cc)
OBJS = $(SRCS:.cc=.o)

all: pkgadd pkgadd.8 pkgadd.conf.5 pkgrm.8 pkginfo.1

%: %.pod
	sed "s|@SYSCONFDIR@|${SYSCONFDIR}|g" $< | pod2man --nourls \
		-r ${VERSION} -c "Package Management Utilities" \
		-n $(basename $@) -s $(subst .,,$(suffix $@)) - > $@

.cc.o:
	${CXX} -c ${CXXFLAGS} ${CPPFLAGS} $< -o $@

pkgadd: ${OBJS}
	${LD} $^ ${LDFLAGS} -o $@

check:
	@podchecker *.pod
	@grep -Eiho "https?://[^\"\\'> ]+" *.* | httpx -silent -fc 200 -sc

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/sbin
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	mkdir -p ${DESTDIR}${MANPREFIX}/man5
	mkdir -p ${DESTDIR}${MANPREFIX}/man8
	cp -f pkgadd           ${DESTDIR}${PREFIX}/sbin/
	cp -f pkginfo.1        ${DESTDIR}${MANPREFIX}/man1/
	cp -f pkgadd.conf.5    ${DESTDIR}${MANPREFIX}/man5/
	cp -f pkgadd.8 pkgrm.8 ${DESTDIR}${MANPREFIX}/man8/
	ln -sf pkgadd          ${DESTDIR}${PREFIX}/sbin/pkgrm
	ln -sf ../sbin/pkgadd  ${DESTDIR}${PREFIX}/bin/pkginfo

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/pkginfo
	rm -f ${DESTDIR}${PREFIX}/sbin/pkgadd
	rm -f ${DESTDIR}${PREFIX}/sbin/pkgrm
	rm -f ${DESTDIR}${MANPREFIX}/man1/pkginfo.1
	rm -f ${DESTDIR}${MANPREFIX}/man5/pkgadd.conf.5
	rm -f ${DESTDIR}${MANPREFIX}/man8/pkgadd.8
	rm -f ${DESTDIR}${MANPREFIX}/man8/pkgrm.8

clean:
	rm -f ${OBJS} pkgadd pkgadd.8 pkgadd.conf.5 pkgrm.8 pkginfo.1

.PHONY: all install uninstall clean
