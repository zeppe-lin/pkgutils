include config.mk

SRCS = $(wildcard *.cc)
OBJS = $(SRCS:.cc=.o)

all: pkgadd pkgadd.8 pkgrm.8 pkginfo.8

%: %.pod
	pod2man --nourls -r $(VERSION) -c ' ' -n $(basename $@) -s 8 $< > $@

.cc.o:
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $< -o $@

pkgadd: $(OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@

install: all
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man8/
	install -m 755 -D pkgadd $(DESTDIR)$(BINDIR)/
	install -m 644 -D -t $(DESTDIR)$(MANDIR)/man8/ pkgadd.8 pkgrm.8 pkginfo.8
	ln -sf pkgadd $(DESTDIR)$(BINDIR)/pkgrm
	ln -sf pkgadd $(DESTDIR)$(BINDIR)/pkginfo

uninstall:
	(cd $(DESTDIR)$(BINDIR)       && rm pkgadd   pkgrm   pkginfo)
	(cd $(DESTDIR)$(MANDIR)/man8/ && rm pkgadd.8 pkgrm.8 pkginfo.8)

clean:
	rm -f $(OBJS) pkgadd pkgadd.8 pkgrm.8 pkginfo.8

.PHONY: all install uninstall clean
