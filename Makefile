.POSIX:

include config.mk

SRCS = $(wildcard *.cc)
OBJS = $(SRCS:.cc=.o)

BIN1 = pkginfo
BIN8 = pkgadd pkgrm
MAN1 = pkginfo.1
MAN5 = pkgadd.conf.5
MAN8 = pkgadd.8 pkgrm.8

all: pkgadd ${MAN1} ${MAN5} ${MAN8}

%: %.pod
	pod2man -r "${NAME} ${VERSION}" -c "${DESCRIPTION}" \
		-n $(basename $@) -s $(subst .,,$(suffix $@)) $< > $@

.cc.o:
	${CXX} -c ${CXXFLAGS} ${CPPFLAGS} $< -o $@

pkgadd: ${OBJS}
	${LD} ${OBJS} ${LDFLAGS} -o $@
	ln -sf pkgadd pkgrm
	ln -sf pkgadd pkginfo

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/sbin
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	mkdir -p ${DESTDIR}${MANPREFIX}/man5
	mkdir -p ${DESTDIR}${MANPREFIX}/man8
	cp -f pkgadd  ${DESTDIR}${PREFIX}/sbin/
	cp -f ${MAN1} ${DESTDIR}${MANPREFIX}/man1/
	cp -f ${MAN5} ${DESTDIR}${MANPREFIX}/man5/
	cp -f ${MAN8} ${DESTDIR}${MANPREFIX}/man8/
	cd ${DESTDIR}${PREFIX}/sbin    && chmod 0755 pkgadd
	cd ${DESTDIR}${MANPREFIX}/man1 && chmod 0644 ${MAN1}
	cd ${DESTDIR}${MANPREFIX}/man5 && chmod 0644 ${MAN5}
	cd ${DESTDIR}${MANPREFIX}/man8 && chmod 0644 ${MAN8}
	ln -sf pkgadd         ${DESTDIR}${PREFIX}/sbin/pkgrm
	ln -sf ../sbin/pkgadd ${DESTDIR}${PREFIX}/bin/pkginfo

install-bashcomp:
	mkdir -p ${DESTDIR}${BASHCOMPDIR}
	cp bash_completion ${DESTDIR}${BASHCOMPDIR}/pkgadd
	ln -s pkgadd ${DESTDIR}${BASHCOMPDIR}/pkginfo
	ln -s pkgadd ${DESTDIR}${BASHCOMPDIR}/pkgrm

uninstall:
	cd ${DESTDIR}${PREFIX}/bin     && rm -f ${BIN1}
	cd ${DESTDIR}${PREFIX}/sbin    && rm -f ${BIN8}
	cd ${DESTDIR}${MANPREFIX}/man1 && rm -f ${MAN1}
	cd ${DESTDIR}${MANPREFIX}/man5 && rm -f ${MAN5}
	cd ${DESTDIR}${MANPREFIX}/man8 && rm -f ${MAN8}

uninstall-bashcomp:
	rm -f ${DESTDIR}${BASHCOMPDIR}/pkgadd
	rm -f ${DESTDIR}${BASHCOMPDIR}/pkginfo
	rm -f ${DESTDIR}${BASHCOMPDIR}/pkgrm

clean:
	rm -f ${OBJS} ${BIN1} ${BIN8} ${MAN1} ${MAN5} ${MAN8}
	rm -f ${DIST}.tar.gz

dist: clean
	git archive --format=tar.gz -o ${DIST}.tar.gz --prefix=${DIST}/ HEAD

.PHONY: all install install-bashcomp uninstall uninstall-bashcomp clean dist
