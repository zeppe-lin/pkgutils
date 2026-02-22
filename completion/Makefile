.POSIX:

include ../config.mk

all:

install:
	mkdir -p $(DESTDIR)$(BASHCOMPDIR)
	cp -f bash_completion $(DESTDIR)$(BASHCOMPDIR)/pkgadd
	ln -sf pkgadd $(DESTDIR)$(BASHCOMPDIR)/pkgrm
	ln -sf pkgadd $(DESTDIR)$(BASHCOMPDIR)/pkgchk
	ln -sf pkgadd $(DESTDIR)$(BASHCOMPDIR)/pkginfo

uninstall:
	cd $(DESTDIR)$(BASHCOMPDIR) && rm -f pkgadd pkgrm pkgchk pkginfo

clean:

.PHONY: all install uninstall clean
