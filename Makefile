include config.mk

all: pkgadd symlinks

OBJS = src/main.o src/pkgadd.o src/pkginfo.o src/pkgrm.o src/pkgutil.o

pkgadd: ${OBJS}
	${CXX} $^ ${LDFLAGS} -o $@

symlinks: pkgadd
	ln -sf pkgadd pkgrm
	ln -sf pkgadd pkginfo

install: all
	mkdir -p                ${DESTDIR}${PREFIX}/bin
	mkdir -p                ${DESTDIR}${PREFIX}/sbin
	mkdir -p                ${DESTDIR}${MANPREFIX}/man1
	mkdir -p                ${DESTDIR}${MANPREFIX}/man5
	mkdir -p                ${DESTDIR}${MANPREFIX}/man8
	cp -f pkgadd            ${DESTDIR}${PREFIX}/sbin/
	cp -f man/pkginfo.1     ${DESTDIR}${MANPREFIX}/man1/
	cp -f man/pkgadd.conf.5 ${DESTDIR}${MANPREFIX}/man5/
	cp -f man/pkgadd.8      ${DESTDIR}${MANPREFIX}/man8/
	cp -f man/pkgrm.8       ${DESTDIR}${MANPREFIX}/man8/
	chmod 0755              ${DESTDIR}${PREFIX}/sbin/pkgadd
	chmod 0644              ${DESTDIR}${MANPREFIX}/man1/pkginfo.1
	chmod 0644              ${DESTDIR}${MANPREFIX}/man5/pkgadd.conf.5
	chmod 0644              ${DESTDIR}${MANPREFIX}/man8/pkgadd.8
	chmod 0644              ${DESTDIR}${MANPREFIX}/man8/pkgrm.8
	ln -sf pkgadd           ${DESTDIR}${PREFIX}/sbin/pkgrm
	ln -sf ../sbin/pkgadd   ${DESTDIR}${PREFIX}/bin/pkginfo

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/pkginfo
	rm -f ${DESTDIR}${PREFIX}/sbin/pkgadd
	rm -f ${DESTDIR}${PREFIX}/sbin/pkgrm
	rm -f ${DESTDIR}${MANPREFIX}/man1/pkginfo.1
	rm -f ${DESTDIR}${MANPREFIX}/man5/pkgadd.conf.5
	rm -f ${DESTDIR}${MANPREFIX}/man8/pkgadd.8
	rm -f ${DESTDIR}${MANPREFIX}/man8/pkgrm.8

install_bashcomp:
	mkdir -p              ${DESTDIR}${BASHCOMPDIR}
	cp -f bash_completion ${DESTDIR}${BASHCOMPDIR}/pkgadd
	ln -sf pkgadd         ${DESTDIR}${BASHCOMPDIR}/pkginfo
	ln -sf pkgadd         ${DESTDIR}${BASHCOMPDIR}/pkgrm

uninstall_bashcomp:
	rm -f ${DESTDIR}${BASHCOMPDIR}/pkgadd
	rm -f ${DESTDIR}${BASHCOMPDIR}/pkginfo
	rm -f ${DESTDIR}${BASHCOMPDIR}/pkgrm

install_vimfiles:
	mkdir -p ${DESTDIR}${VIMFILESDIR}/ftdetect
	mkdir -p ${DESTDIR}${VIMFILESDIR}/syntax
	cp -f vim/ftdetect/pkgaddconf.vim ${DESTDIR}${VIMFILESDIR}/ftdetect/
	cp -f vim/syntax/pkgaddconf.vim   ${DESTDIR}${VIMFILESDIR}/syntax/

uninstall_vimfiles:
	rm -f ${DESTDIR}${VIMFILESDIR}/ftdetect/pkgaddconf.vim
	rm -f ${DESTDIR}${VIMFILESDIR}/syntax/pkgadd.conf

clean:
	rm -f ${OBJS} pkginfo pkgadd pkgrm
	rm -f ${DIST}.tar.gz

dist: clean
	git archive --format=tar.gz -o ${DIST}.tar.gz --prefix=${DIST}/ HEAD

.PHONY: all install uninstall install_bashcomp uninstall_bashcomp \
	install_vimfiles uninstall_vimfiles clean dist
