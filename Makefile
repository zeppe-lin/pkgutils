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
	@echo "=======> Check URLs for response code"
	@grep -Eiho "https?://[^\"\\'> ]+" *.* | xargs -P10 -I{} \
		curl -o /dev/null -sw "[%{http_code}] %{url}\n" '{}'

install-dirs:
	mkdir -p ${DESTDIR}/usr/bin
	mkdir -p ${DESTDIR}/usr/sbin
	mkdir -p ${DESTDIR}/usr/share/man/man1
	mkdir -p ${DESTDIR}/usr/share/man/man5
	mkdir -p ${DESTDIR}/usr/share/man/man8

install: all install-dirs
	cp -f pkgadd           ${DESTDIR}/usr/sbin/
	chmod 0755             ${DESTDIR}/usr/sbin/pkgadd
	cp -f pkginfo.1        ${DESTDIR}/usr/share/man/man1/
	chmod 0644             ${DESTDIR}/usr/share/man/man1/pkgadd
	cp -f pkgadd.conf.5    ${DESTDIR}/usr/share/man/man5/
	chmod 0644             ${DESTDIR}/usr/share/man/man5/pkgadd.conf.5
	cp -f pkgadd.8 pkgrm.8 ${DESTDIR}/usr/share/man/man8/
	chmod 0644             ${DESTDIR}/usr/share/man/man8/pkgadd.8
	chmod 0644             ${DESTDIR}/usr/share/man/man8/pkgrm.8
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
