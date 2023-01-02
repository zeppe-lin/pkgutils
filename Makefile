.POSIX:

include config.mk

SRCS = $(wildcard *.cc)
OBJS = $(SRCS:.cc=.o)

all: pkgadd pkgadd.8 pkgadd.conf.5 pkgrm.8 pkginfo.1

%: %.pod
	pod2man --nourls -r ${VERSION} -c "Package Management Utilities" \
		-n $(basename $@) -s $(subst .,,$(suffix $@)) $< > $@

.cc.o:
	${CXX} -c ${CXXFLAGS} ${CPPFLAGS} $< -o $@

pkgadd: ${OBJS}
	${LD} $^ ${LDFLAGS} -o $@

check:
	@echo "=======> Check PODs for errors"
	@podchecker *.pod
	@echo "=======> Check URLs for non-200 response code"
	@grep -Eiho "https?://[^\"\\'> ]+" *.* | httpx -silent -fc 200 -sc

install: all
	mkdir -p ${DESTDIR}/usr/bin
	mkdir -p ${DESTDIR}/usr/sbin
	mkdir -p ${DESTDIR}/usr/share/man/man1
	mkdir -p ${DESTDIR}/usr/share/man/man5
	mkdir -p ${DESTDIR}/usr/share/man/man8
	cp -f pkgadd           ${DESTDIR}/usr/sbin/
	cp -f pkginfo.1        ${DESTDIR}/usr/share/man/man1/
	cp -f pkgadd.conf.5    ${DESTDIR}/usr/share/man/man5/
	cp -f pkgadd.8 pkgrm.8 ${DESTDIR}/usr/share/man/man8/
	ln -sf pkgadd          ${DESTDIR}/usr/sbin/pkgrm
	ln -sf ../sbin/pkgadd  ${DESTDIR}/usr/bin/pkginfo

uninstall:
	rm -f ${DESTDIR}/usr/bin/pkginfo
	rm -f ${DESTDIR}/usr/sbin/pkgadd
	rm -f ${DESTDIR}/usr/sbin/pkgrm
	rm -f ${DESTDIR}/usr/share/man/man1/pkginfo.1
	rm -f ${DESTDIR}/usr/share/man/man5/pkgadd.conf.5
	rm -f ${DESTDIR}/usr/share/man/man8/pkgadd.8
	rm -f ${DESTDIR}/usr/share/man/man8/pkgrm.8

clean:
	rm -f ${OBJS} pkgadd pkgadd.8 pkgadd.conf.5 pkgrm.8 pkginfo.1

.PHONY: all install uninstall clean
