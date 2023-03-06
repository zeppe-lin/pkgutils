.POSIX:

include config.mk

SRCS = $(wildcard *.cc)
OBJS = $(SRCS:.cc=.o)

all: pkgadd pkgadd.8 pkgadd.conf.5 pkgrm.8 pkginfo.1

%: %.pod
	pod2man --nourls -r "pkgutils ${VERSION}" \
		-c "Package Management Utilities" \
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
	mkdir -p ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/sbin
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	mkdir -p ${DESTDIR}${MANPREFIX}/man5
	mkdir -p ${DESTDIR}${MANPREFIX}/man8

install: all install-dirs
	cp -f pkgadd           ${DESTDIR}${PREFIX}/sbin/
	cp -f pkginfo.1        ${DESTDIR}${MANPREFIX}/man1/
	cp -f pkgadd.conf.5    ${DESTDIR}${MANPREFIX}/man5/
	cp -f pkgadd.8 pkgrm.8 ${DESTDIR}${MANPREFIX}/man8/
	ln -sf pkgadd          ${DESTDIR}${PREFIX}/sbin/pkgrm
	ln -sf ../sbin/pkgadd  ${DESTDIR}${PREFIX}/bin/pkginfo
	chmod 0755             ${DESTDIR}${PREFIX}/sbin/pkgadd
	chmod 0644             ${DESTDIR}${MANPREFIX}/man1/pkginfo.1
	chmod 0644             ${DESTDIR}${MANPREFIX}/man5/pkgadd.conf.5
	chmod 0644             ${DESTDIR}${MANPREFIX}/man8/pkgadd.8
	chmod 0644             ${DESTDIR}${MANPREFIX}/man8/pkgrm.8

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

.PHONY: all check install-dirs install uninstall clean
