# See COPYING and COPYRIGHT files for corresponding information.

include config.mk

SRCS = $(wildcard *.cc)
OBJS = $(SRCS:.cc=.o)

all: pkgadd pkgadd.8 pkgrm.8 pkginfo.8

%: %.pod
	pod2man --nourls -r ${VERSION} -c ' ' -n $(basename $@) \
		-s 8 $<  >  $@

.cc.o:
	${CXX} -c ${CXXFLAGS} ${CPPFLAGS} $< -o $@

pkgadd: ${OBJS}
	${LD} $^ ${LDFLAGS} -o $@

install: all
	mkdir -p ${DESTDIR}${BINDIR}
	mkdir -p ${DESTDIR}${MANDIR}/man8
	cp -f pkgadd ${DESTDIR}${BINDIR}/
	cp -f pkgadd.8 pkgrm.8 pkginfo.8 ${DESTDIR}${MANDIR}/man8/
	ln -sf pkgadd ${DESTDIR}${BINDIR}/pkgrm
	ln -sf pkgadd ${DESTDIR}${BINDIR}/pkginfo

uninstall:
	(cd ${DESTDIR}${BINDIR}       && rm pkgadd   pkgrm   pkginfo)
	(cd ${DESTDIR}${MANDIR}/man8/ && rm pkgadd.8 pkgrm.8 pkginfo.8)

clean:
	rm -f ${OBJS} pkgadd pkgadd.8 pkgrm.8 pkginfo.8

.PHONY: all install uninstall clean

# vim:cc=72:tw=70
# End of file.
