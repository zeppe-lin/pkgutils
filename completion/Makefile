.POSIX:

include ../config.mk

all:

install:
	mkdir -p $(DESTDIR)$(BASHCOMPDIR)
	cp -f bash_completion $(DESTDIR)$(BASHCOMPDIR)/pkgadd
	ln -sf pkgadd $(DESTDIR)$(BASHCOMPDIR)/pkginfo
	ln -sf pkgadd $(DESTDIR)$(BASHCOMPDIR)/pkgrm

uninstall:
	cd $(DESTDIR)$(BASHCOMPDIR) && rm -f pkgadd pkginfo pkgrm

clean:

.PHONY: all install uninstall clean
