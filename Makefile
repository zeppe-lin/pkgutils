.POSIX:

include config.mk

OBJS = $(subst .cc,.o,$(wildcard src/*.cc))
BIN1 = pkginfo
BIN8 = pkgadd pkgrm
MAN1 = $(subst .1.pod,.1,$(wildcard pod/*.1.pod))
MAN5 = $(subst .5.pod,.5,$(wildcard pod/*.5.pod))
MAN8 = $(subst .8.pod,.8,$(wildcard pod/*.8.pod))

all: pkgadd manpages

%: %.pod
	pod2man -r "${NAME} ${VERSION}" -c "${DESCRIPTION}" \
		-n $(basename $@) -s $(subst .,,$(suffix $@)) $< > $@

.cc.o:
	${CXX} -c ${CXXFLAGS} ${CPPFLAGS} $< -o $@

pkgadd: ${OBJS}
	${LD} ${OBJS} ${LDFLAGS} -o $@
	ln -sf pkgadd pkgrm
	ln -sf pkgadd pkginfo

manpages: ${MAN1} ${MAN5} ${MAN8}

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
	cd ${DESTDIR}${MANPREFIX}/man1 && chmod 0644 ${MAN1:pod/%=%}
	cd ${DESTDIR}${MANPREFIX}/man5 && chmod 0644 ${MAN5:pod/%=%}
	cd ${DESTDIR}${MANPREFIX}/man8 && chmod 0644 ${MAN8:pod/%=%}
	ln -sf pkgadd         ${DESTDIR}${PREFIX}/sbin/pkgrm
	ln -sf ../sbin/pkgadd ${DESTDIR}${PREFIX}/bin/pkginfo

uninstall:
	cd ${DESTDIR}${PREFIX}/bin     && rm -f ${BIN1}
	cd ${DESTDIR}${PREFIX}/sbin    && rm -f ${BIN8}
	cd ${DESTDIR}${MANPREFIX}/man1 && rm -f ${MAN1:pod/%=%}
	cd ${DESTDIR}${MANPREFIX}/man5 && rm -f ${MAN5:pod/%=%}
	cd ${DESTDIR}${MANPREFIX}/man8 && rm -f ${MAN8:pod/%=%}

install-bashcomp:
	mkdir -p ${DESTDIR}${BASHCOMPDIR}
	cp -f bash_completion ${DESTDIR}${BASHCOMPDIR}/pkgadd
	ln -sf pkgadd ${DESTDIR}${BASHCOMPDIR}/pkginfo
	ln -sf pkgadd ${DESTDIR}${BASHCOMPDIR}/pkgrm

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
